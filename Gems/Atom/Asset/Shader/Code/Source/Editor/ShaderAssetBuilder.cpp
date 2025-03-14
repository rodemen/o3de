/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "ShaderAssetBuilder.h"

#include <CommonFiles/Preprocessor.h>
#include <CommonFiles/GlobalBuildOptions.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderAssetCreator.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>
#include <Atom/RPI.Reflect/Shader/ShaderVariantKey.h>

#include <Atom/RHI.Edit/Utils.h>
#include <Atom/RHI.Edit/ShaderPlatformInterface.h>
#include <Atom/RPI.Edit/Common/JsonReportingHelper.h>
#include <Atom/RPI.Edit/Common/AssetUtils.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>
#include <Atom/RHI.Reflect/ConstantsLayout.h>
#include <Atom/RHI.Reflect/PipelineLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ShaderStageFunction.h>

#include <AzCore/Serialization/Json/JsonUtils.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/Debug/TraceContext.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Platform/PlatformDefaults.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/JSON/document.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/IOUtils.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/sort.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Debug/Timer.h>

#include "AzslCompiler.h"
#include "ShaderVariantAssetBuilder.h"
#include "ShaderBuilderUtility.h"
#include "ShaderPlatformInterfaceRequest.h"
#include "AtomShaderConfig.h"

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/SerializationDependencies.h>
namespace AZ
{
    namespace ShaderBuilder
    {
        static constexpr char ShaderAssetBuilderName[] = "ShaderAssetBuilder";
        static constexpr uint32_t ShaderAssetBuildTimestampParam = 0;

        void ShaderAssetBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const
        {
            AZStd::string fullPath;
            AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.data(), request.m_sourceFile.data(), fullPath, true);

            AZ_TracePrintf(ShaderAssetBuilderName, "CreateJobs for Shader \"%s\"\n", fullPath.data());

            // Used to synchronize versions of the ShaderAsset and ShaderVariantTreeAsset, especially during hot-reload.
            // Note it's probably important for this to be set once outside the platform loop so every platform's ShaderAsset
            // has the same value, because later the ShaderVariantTreeAsset job will fetch this value from the local ShaderAsset
            // which could cross platforms (i.e. building an android ShaderVariantTreeAsset on PC would fetch the tiemstamp from
            // the PC's ShaderAsset).
            AZStd::sys_time_t shaderAssetBuildTimestamp = AZStd::GetTimeNowMicroSecond();

            // Need to get the name of the azsl file from the .shader source asset, to be able to declare a dependency to SRG Layout Job.
            // and the macro options to preprocess.
            auto descriptorParseOutcome = ShaderBuilderUtility::LoadShaderDataJson(fullPath);
            if (!descriptorParseOutcome.IsSuccess())
            {
                AZ_Error(
                    ShaderAssetBuilderName, false, "Failed to parse Shader Descriptor JSON: %s",
                    descriptorParseOutcome.GetError().c_str());
                return;
            }

            RPI::ShaderSourceData shaderSourceData = descriptorParseOutcome.TakeValue();

            AZStd::string azslFullPath;
            ShaderBuilderUtility::GetAbsolutePathToAzslFile(fullPath, shaderSourceData.m_source, azslFullPath);
            if (!IO::FileIOBase::GetInstance()->Exists(azslFullPath.c_str()))
            {
                AZ_Error(
                    ShaderAssetBuilderName, false, "Shader program listed as the source entry does not exist: %s.", azslFullPath.c_str());
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                return;
            }


            GlobalBuildOptions buildOptions = ReadBuildOptions(ShaderAssetBuilderName);

            // [GFX TODO] [ATOM-14966] In principle, based on macro definitions, included files can change per supervariant.
            //                         So, the list of source asset dependencies must be collected by running MCPP on each supervariant.
            //                         For now, we will run MCPP only once because CreateJobs() should be as light as possible.
            // 
            // Regardless of the PlatformInfo and enabled ShaderPlatformInterfaces, the azsl file will be preprocessed
            // with the sole purpose of extracting all included files. For each included file a SourceDependency will be declared.
            PreprocessorData output;
            buildOptions.m_compilerArguments.Merge(shaderSourceData.m_compiler);
            PreprocessFile(azslFullPath, output, buildOptions.m_preprocessorSettings, true, true);
            for (auto includePath : output.includedPaths)
            {
                // m_sourceFileDependencyList does not support paths with "." or ".." for relative lookup, but the preprocessor
                // may produce path strings like "C:/a/b/c/../../d/file.azsli" so we have to normalize
                AzFramework::StringFunc::Path::Normalize(includePath);

                AssetBuilderSDK::SourceFileDependency includeFileDependency;
                includeFileDependency.m_sourceFileDependencyPath = includePath;
                response.m_sourceFileDependencyList.emplace_back(includeFileDependency);
            }

            {
                // Add the AZSL as source dependency
                AssetBuilderSDK::SourceFileDependency azslFileDependency;
                azslFileDependency.m_sourceFileDependencyPath = azslFullPath;
                response.m_sourceFileDependencyList.emplace_back(azslFileDependency);
            }

            for (const AssetBuilderSDK::PlatformInfo& platformInfo : request.m_enabledPlatforms)
            {
                AZ_TraceContext("For platform", platformInfo.m_identifier.data());

                // Get the platform interfaces to be able to access the prepend file
                AZStd::vector<RHI::ShaderPlatformInterface*> platformInterfaces = ShaderBuilderUtility::DiscoverValidShaderPlatformInterfaces(platformInfo);
                if (platformInterfaces.empty())
                {
                    continue;
                }

                AssetBuilderSDK::JobDescriptor jobDescriptor;
                jobDescriptor.m_priority = 2;
                jobDescriptor.m_critical = false;
                jobDescriptor.m_jobKey = ShaderAssetBuilderJobKey;
                jobDescriptor.SetPlatformIdentifier(platformInfo.m_identifier.c_str());
                jobDescriptor.m_jobParameters.emplace(ShaderAssetBuildTimestampParam, AZStd::to_string(shaderAssetBuildTimestamp));

                response.m_createJobOutputs.push_back(jobDescriptor);
            }  // for all request.m_enabledPlatforms

            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        }

        static bool SerializeOutShaderAsset(Data::Asset<RPI::ShaderAsset> shaderAsset,
            const AZStd::string& tempDirPath,
            AssetBuilderSDK::ProcessJobResponse& response)
        {
            AZStd::string shaderAssetFileName = AZStd::string::format("%s.%s", shaderAsset->GetName().GetCStr(), RPI::ShaderAsset::Extension);
            AZStd::string shaderAssetOutputPath;
            AzFramework::StringFunc::Path::ConstructFull(tempDirPath.data(), shaderAssetFileName.data(), shaderAssetOutputPath, true);
        
            if (!Utils::SaveObjectToFile(shaderAssetOutputPath, DataStream::ST_BINARY, shaderAsset.Get()))
            {
                AZ_Error(ShaderAssetBuilderName, false, "Failed to output Shader Descriptor");
                return false;
            }
        
            AssetBuilderSDK::JobProduct shaderJobProduct;
            if (!AssetBuilderSDK::OutputObject(shaderAsset.Get(), shaderAssetOutputPath, azrtti_typeid<RPI::ShaderAsset>(),
                aznumeric_cast<uint32_t>(RPI::ShaderAssetSubId::ShaderAsset), shaderJobProduct))
            {
                AZ_Error(ShaderAssetBuilderName, false, "Failed to output product dependencies.");
                return false;
            }
            response.m_outputProducts.push_back(AZStd::move(shaderJobProduct));
        
            return true;
        }

        static AZ::Outcome<RHI::ShaderStageAttributeMapList, AZStd::string> BuildAttributesMap(
            const RHI::ShaderPlatformInterface* shaderPlatformInterface,
            const AzslData& azslData,
            const MapOfStringToStageType& shaderEntryPoints,
            bool& hasRasterProgram)
        {
            hasRasterProgram = false;
            bool hasComputeProgram = false;
            bool hasRayTracingProgram = false;
            RHI::ShaderStageAttributeMapList attributeMaps;
            attributeMaps.resize(RHI::ShaderStageCount);
            for (const auto& shaderEntryPoint : shaderEntryPoints)
            {
                auto shaderEntryName = shaderEntryPoint.first;
                auto shaderStageType = shaderEntryPoint.second;
                auto assetBuilderShaderType = ShaderBuilderUtility::ToAssetBuilderShaderType(shaderStageType);
                hasRasterProgram |= shaderPlatformInterface->IsShaderStageForRaster(assetBuilderShaderType);
                hasComputeProgram |= shaderPlatformInterface->IsShaderStageForCompute(assetBuilderShaderType);
                hasRayTracingProgram |= shaderPlatformInterface->IsShaderStageForRayTracing(assetBuilderShaderType);

                auto findId = AZStd::find_if(AZ_BEGIN_END(azslData.m_functions), [&shaderEntryPoint](const auto& func) {
                    return func.m_name == shaderEntryPoint.first;
                });

                if (findId == azslData.m_functions.end())
                {
                    // shaderData.m_functions only contains Vertex, Fragment and Compute entries for now
                    // Tessellation shaders will need to be handled too
                    continue;
                }

                const auto shaderStage = ToRHIShaderStage(assetBuilderShaderType);
                for (const auto& attr : findId->attributesList)
                {
                    // Some stages like RHI::ShaderStage::Tessellation are compound and consist of two or more shader entries
                    const Name& attributeName = attr.first;
                    const RHI::ShaderStageAttributeArguments& args = attr.second;
                    const auto stageIndex = static_cast<uint32_t>(shaderStage);
                    AZ_Assert(stageIndex < RHI::ShaderStageCount, "Invalid shader stage specified!");
                    attributeMaps[stageIndex][attributeName] = args;
                }
            }

            if (hasRasterProgram && hasComputeProgram)
            {
                return AZ::Failure(AZStd::string(" Shader asset descriptor defines both a raster entry point and a compute entry point."));
            }

            if (!hasRasterProgram && !hasComputeProgram && !hasRayTracingProgram)
            {
                return AZ::Failure(
                    AZStd::string( "Shader asset descriptor has a program variant that does not define any entry points."
                        " Please declare entry points in the .shader file."));
            }

            return AZ::Success(attributeMaps);
        }

        void ShaderAssetBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
        {
            AZ::Debug::Timer timer;
            timer.Stamp();

            AZStd::string shaderFullPath;
            AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), shaderFullPath, true);
            // Save .shader file name (no extension and no parent directory path)
            AZStd::string shaderFileName;
            AzFramework::StringFunc::Path::GetFileName(request.m_sourceFile.c_str(), shaderFileName);

            // No error checking because the same calls were already executed during CreateJobs()
            auto descriptorParseOutcome = ShaderBuilderUtility::LoadShaderDataJson(shaderFullPath);
            RPI::ShaderSourceData shaderSourceData = descriptorParseOutcome.TakeValue();
            AZStd::string azslFullPath;
            ShaderBuilderUtility::GetAbsolutePathToAzslFile(shaderFullPath, shaderSourceData.m_source, azslFullPath);
            AZ_TracePrintf(ShaderAssetBuilderName, "Original AZSL File: %s \n", azslFullPath.c_str());

            // The directory where the Azsl file was found must be added to the list of include paths
            AZStd::string azslFolderPath;
            AzFramework::StringFunc::Path::GetFolderPath(azslFullPath.c_str(), azslFolderPath);
            GlobalBuildOptions buildOptions = ReadBuildOptions(ShaderAssetBuilderName, azslFolderPath.c_str());

            // Request the list of valid shader platform interfaces for the target platform.
            AZStd::vector<RHI::ShaderPlatformInterface*> platformInterfaces = ShaderBuilderUtility::DiscoverEnabledShaderPlatformInterfaces(
                request.m_platformInfo, shaderSourceData);
            if (platformInterfaces.empty())
            {
                //No work to do. Exit gracefully.
                AZ_TracePrintf(ShaderAssetBuilderName,
                    "No azshader is produced on behalf of %s because all valid RHI backends were disabled for this shader.\n",
                    shaderFullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
                return;
            }

            // Get the time stamp string as sys_time_t, and also convert back to string to make sure it was converted correctly.
            AZStd::sys_time_t shaderAssetBuildTimestamp = 0;
            auto shaderAssetBuildTimestampIterator = request.m_jobDescription.m_jobParameters.find(ShaderAssetBuildTimestampParam);
            if (shaderAssetBuildTimestampIterator != request.m_jobDescription.m_jobParameters.end())
            {
                shaderAssetBuildTimestamp = AZStd::stoull(shaderAssetBuildTimestampIterator->second);

                if (AZStd::to_string(shaderAssetBuildTimestamp) != shaderAssetBuildTimestampIterator->second)
                {
                    response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                    AZ_Assert(false, "Incorrect conversion of ShaderAssetBuildTimestampParam");
                    return;
                }
            }

            auto supervariantList = ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData);

            RPI::ShaderAssetCreator shaderAssetCreator;
            shaderAssetCreator.Begin(Uuid::CreateRandom());

            shaderAssetCreator.SetName(AZ::Name{shaderFileName.c_str()});
            shaderAssetCreator.SetDrawListName(Name(shaderSourceData.m_drawListName));
            shaderAssetCreator.SetShaderAssetBuildTimestamp(shaderAssetBuildTimestamp);

            // The ShaderOptionGroupLayout must be the same across all supervariants because
            // there can be only a single ShaderVariantTreeAsset per ShaderAsset.
            // We will store here the one that results when the *.azslin file is
            // compiled for the default, nameless, supervariant.
            // For all other supervariants we just make sure the hashes are the same
            // as this one.
            RPI::Ptr<RPI::ShaderOptionGroupLayout> finalShaderOptionGroupLayout = nullptr;


            // Time to describe the big picture.
            // 1- Preprocess an AZSL file with MCPP (a C-Preprocessor), and generate a flat AZSL file without #include lines and any macros in it.
            //    Let's call it the Flat-AZSL file. There are two levels of macro definition that need to be merged before we can invoke MCPP:
            //    1.1-  From <GameProject>/Config/shader_global_build_options.json, which we have stored in the local variable @buildOptions.
            //    1.2-  From the "Supervariant" definition key, which can be different for each supervariant. 
            // 2- There will be one Flat-AZSL per supervariant. Each Flat-AZSL will be transpiled to HLSL with AZSLc. This means there will be one HLSL file
            //    per supervariant.
            // 3- The generated HLSL (one HLSL per supervariant) file may contain C-Preprocessor Macros inserted by AZSLc. And that file will be given to DXC.
            //    DXC has a preprocessor embedded in it.  DXC will be executed once for each entry function listed in the .shader file.
            //    There will be one DXIL compiled binary for each entry function. All the DXIL compiled binaries for each supervariant will be combined
            //    in the ROOT ShaderVariantAsset.
            
            // Remark: In general, the work done by the ShaderVariantAssetBuilder is similar, but it will start from the HLSL file created; in step 2, mentioned above; by this builder,
            // for each supervariant. 

            // At this moment We have global build options that should be merged with the build options that are common
            // to all the supervariants of this shader.
            buildOptions.m_compilerArguments.Merge(shaderSourceData.m_compiler);

            for (RHI::ShaderPlatformInterface* shaderPlatformInterface : platformInterfaces)
            {
                AZStd::string apiName(shaderPlatformInterface->GetAPIName().GetCStr());
                AZ_TraceContext("Platform API", apiName);
                // Signal the begin of shader data for an RHI API.
                shaderAssetCreator.BeginAPI(shaderPlatformInterface->GetAPIType());

                // Each shaderPlatformInterface has its own azsli header that needs to be prepended to the AZSL file before
                // preprocessing. We will create a new temporary file that contains the combined data.
                RHI::PrependArguments args;
                args.m_sourceFile = azslFullPath.c_str();
                args.m_prependFile = shaderPlatformInterface->GetAzslHeader(request.m_platformInfo);
                args.m_addSuffixToFileName = apiName.c_str();
                args.m_destinationFolder = request.m_tempDirPath.c_str();

                AZStd::string prependedAzslFilePath = RHI::PrependFile(args);
                if (prependedAzslFilePath == azslFullPath)
                {
                    // The specific error is already reported by RHI::PrependFile().
                    response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                    return;
                }

                // Cache common AZSLC invokation arguments related with the current RHI Backend.
                // Each supervariant can, optionally, remove or add more arguments for AZSLc.
                AZStd::string commonAzslcCompilerParameters =
                    shaderPlatformInterface->GetAzslCompilerParameters(buildOptions.m_compilerArguments);
                commonAzslcCompilerParameters += " ";
                commonAzslcCompilerParameters +=
                    shaderPlatformInterface->GetAzslCompilerWarningParameters(buildOptions.m_compilerArguments);
                AtomShaderConfig::AddParametersFromConfigFile(commonAzslcCompilerParameters, request.m_platformInfo);

                // The register number only makes sense if the platform uses "spaces",
                // since the register Id of the resource will not change even if the pipeline layout changes.
                // We can pass in a default ShaderCompilerArguments because all we care about is whether the shaderPlatformInterface
                // appends the "--use-spaces" flag.
                const bool platformUsesRegisterSpaces =
                    (AzFramework::StringFunc::Find(commonAzslcCompilerParameters, "--use-spaces") != AZStd::string::npos);

                uint32_t supervariantIndex = 0;
                for (const auto& supervariantInfo : supervariantList)
                {
                    AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);
                    if (jobCancelListener.IsCancelled())
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                        return;
                    }

                    shaderAssetCreator.BeginSupervariant(supervariantInfo.m_name);

                    // Let's combine the global macro definitions, with the macro definitions particular to this
                    // supervariant. Two steps:
                    // 1- Supervariants can specify which macros to remove from the global definitions.
                    AZStd::vector<AZStd::string> macroDefinitionNamesToRemove = supervariantInfo.GetCombinedListOfMacroDefinitionNamesToRemove();
                    PreprocessorOptions preprocessorOptions = buildOptions.m_preprocessorSettings;
                    preprocessorOptions.RemovePredefinedMacros(macroDefinitionNamesToRemove);
                    // 2- Supervariants can specify which macros to add.
                    AZStd::vector<AZStd::string> macroDefinitionsToAdd = supervariantInfo.GetMacroDefinitionsToAdd();
                    preprocessorOptions.m_predefinedMacros.insert(
                        preprocessorOptions.m_predefinedMacros.end(), macroDefinitionsToAdd.begin(), macroDefinitionsToAdd.end());
                    // Run the preprocessor.
                    PreprocessorData output;
                    PreprocessFile(prependedAzslFilePath, output, preprocessorOptions, true, true);
                    RHI::ReportErrorMessages(ShaderAssetBuilderName, output.diagnostics);
                    // Dump the preprocessed string as a flat AZSL file with extension .azslin, which will be given to AZSLc to generate the HLSL file.
                    AZStd::string superVariantAzslinStemName = shaderFileName;
                    if (!supervariantInfo.m_name.IsEmpty())
                    {
                        superVariantAzslinStemName += AZStd::string::format("-%s", supervariantInfo.m_name.GetCStr());
                    }
                    AZStd::string azslinFullPath = ShaderBuilderUtility::DumpPreprocessedCode(
                        ShaderAssetBuilderName, output.code, request.m_tempDirPath, superVariantAzslinStemName,
                        apiName);
                    if (azslinFullPath.empty())
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    AZ_TracePrintf(ShaderAssetBuilderName, "Preprocessed AZSL File: %s \n", prependedAzslFilePath.c_str());

                    // Before transpiling the flat-AZSL(.azslin) file into HLSL it is necessary
                    // to setup the AZSLc arguments as required by the current supervariant.
                    AZStd::string azslcCompilerParameters = supervariantInfo.GetCustomizedArgumentsForAzslc(commonAzslcCompilerParameters);

                    // Ready to transpile the azslin file into HLSL.
                    ShaderBuilder::AzslCompiler azslc(azslinFullPath);
                    AZStd::string hlslFullPath = AZStd::string::format("%s_%s.hlsl", superVariantAzslinStemName.c_str(), apiName.c_str());
                    AzFramework::StringFunc::Path::Join(request.m_tempDirPath.c_str(), hlslFullPath.c_str(), hlslFullPath, true);
                    auto emitFullOutcome = azslc.EmitFullData(azslcCompilerParameters, hlslFullPath);
                    if (!emitFullOutcome.IsSuccess())
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    ShaderBuilderUtility::AzslSubProducts::Paths subProductsPaths = emitFullOutcome.TakeValue();

                    // In addition to the hlsl file, there are other json files that were generated.
                    // Each output file will become a product.
                    for (int i = 0; i < subProductsPaths.size(); ++i)
                    {
                        AssetBuilderSDK::JobProduct jobProduct;
                        jobProduct.m_productFileName = subProductsPaths[i];
                        static const AZ::Uuid AzslOutcomeType = "{6977AEB1-17AD-4992-957B-23BB2E85B18B}";
                        jobProduct.m_productAssetType = AzslOutcomeType;
                        // uint32_t rhiApiUniqueIndex, uint32_t supervariantIndex, uint32_t subProductType
                        jobProduct.m_productSubID = RPI::ShaderAsset::MakeProductAssetSubId(
                                                                shaderPlatformInterface->GetAPIUniqueIndex(), supervariantIndex,
                                                                aznumeric_cast<uint32_t>(ShaderBuilderUtility::AzslSubProducts::SubList[i]));
                        jobProduct.m_dependenciesHandled = true;
                        // Note that the output products are not traditional product assets that will be used by the game project.
                        // They are artifacts that are produced once, cached, and used later by other AssetBuilders as a way to centralize
                        // build organization.
                        response.m_outputProducts.push_back(AZStd::move(jobProduct));
                    }

                    AZStd::shared_ptr<ShaderFiles> files(new ShaderFiles);
                    AzslData azslData(files);
                    azslData.m_preprocessedFullPath = azslinFullPath;
                    RPI::ShaderResourceGroupLayoutList srgLayoutList;
                    RPI::Ptr<RPI::ShaderOptionGroupLayout> shaderOptionGroupLayout = RPI::ShaderOptionGroupLayout::Create();
                    BindingDependencies bindingDependencies;
                    RootConstantData rootConstantData;
                    AssetBuilderSDK::ProcessJobResultCode azslJsonReadResult = ShaderBuilderUtility::PopulateAzslDataFromJsonFiles(
                        ShaderAssetBuilderName, subProductsPaths, platformUsesRegisterSpaces, azslData, srgLayoutList, shaderOptionGroupLayout,
                        bindingDependencies, rootConstantData);
                    if (azslJsonReadResult != AssetBuilderSDK::ProcessJobResult_Success)

                    {
                        response.m_resultCode = azslJsonReadResult;
                        return;
                    }

                    shaderAssetCreator.SetSrgLayoutList(srgLayoutList);

                    if (!finalShaderOptionGroupLayout)
                    {
                        finalShaderOptionGroupLayout = shaderOptionGroupLayout;
                        shaderAssetCreator.SetShaderOptionGroupLayout(finalShaderOptionGroupLayout);
                        [[maybe_unused]] const uint32_t usedShaderOptionBits = shaderOptionGroupLayout->GetBitSize();
                        AZ_TracePrintf(
                            ShaderAssetBuilderName, "Note: This shader uses %u of %u available shader variant key bits. \n",
                            usedShaderOptionBits, RPI::ShaderVariantKeyBitCount);
                    }
                    else
                    {
                        if (finalShaderOptionGroupLayout->GetHash() != shaderOptionGroupLayout->GetHash())
                        {
                            AZ_Error(
                                ShaderAssetBuilderName, false, "Supervariant %s has a different ShaderOptionGroupLayout",
                                supervariantInfo.m_name.GetCStr())
                            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                            return;
                        }
                    }

                    if (shaderSourceData.m_programSettings.m_entryPoints.empty())
                    {
                        AZ_Error( ShaderAssetBuilderName, false, "ProgramSettings must specify entry points.");
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }

                    // Discover entry points & type of programs.
                    MapOfStringToStageType shaderEntryPoints;
                    for (const auto& entryPoint : shaderSourceData.m_programSettings.m_entryPoints)
                    {
                        shaderEntryPoints[entryPoint.m_name] = entryPoint.m_type;
                    }

                    bool hasRasterProgram = false;
                    auto attributeMapsOutcome = BuildAttributesMap(shaderPlatformInterface, azslData, shaderEntryPoints, hasRasterProgram);
                    if (!attributeMapsOutcome.IsSuccess())
                    {
                        AZ_Error(ShaderAssetBuilderName, false, "%s\n", attributeMapsOutcome.GetError().c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    shaderAssetCreator.SetShaderStageAttributeMapList(attributeMapsOutcome.TakeValue());

                    // Check if we were canceled before we do any heavy processing of
                    // the shader data (compiling the shader kernels, processing SRG
                    // and pipeline layout data, etc.).
                    if (jobCancelListener.IsCancelled())
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                        return;
                    }

                    RHI::Ptr<RHI::PipelineLayoutDescriptor> pipelineLayoutDescriptor =
                        ShaderBuilderUtility::BuildPipelineLayoutDescriptorForApi(
                            ShaderAssetBuilderName, srgLayoutList, shaderEntryPoints, buildOptions.m_compilerArguments, rootConstantData,
                            shaderPlatformInterface, bindingDependencies);
                    if (!pipelineLayoutDescriptor)
                    {
                        AZ_Error(
                            ShaderAssetBuilderName, false, "Failed to build pipeline layout descriptor for api=[%s]",
                            shaderPlatformInterface->GetAPIName().GetCStr());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }

                    shaderAssetCreator.SetPipelineLayout(pipelineLayoutDescriptor);


                    RPI::ShaderInputContract shaderInputContract;
                    RPI::ShaderOutputContract shaderOutputContract;
                    size_t colorAttachmentCount = 0;
                    ShaderBuilderUtility::CreateShaderInputAndOutputContracts(
                        azslData, shaderEntryPoints, *shaderOptionGroupLayout.get(),
                        subProductsPaths[ShaderBuilderUtility::AzslSubProducts::om],
                        subProductsPaths[ShaderBuilderUtility::AzslSubProducts::ia],
                        shaderInputContract, shaderOutputContract, colorAttachmentCount);
                    shaderAssetCreator.SetInputContract(shaderInputContract);
                    shaderAssetCreator.SetOutputContract(shaderOutputContract);

                    if (hasRasterProgram)
                    {
                        // Set the various states to what is in the descriptor.
                        const RHI::TargetBlendState& targetBlendState = shaderSourceData.m_blendState;
                        RHI::RenderStates renderStates;
                        renderStates.m_rasterState = shaderSourceData.m_rasterState;
                        renderStates.m_depthStencilState = shaderSourceData.m_depthStencilState;
                        // [GFX TODO][ATOM-930] We should support unique blend states per RT
                        for (size_t i = 0; i < colorAttachmentCount; ++i)
                        {
                            renderStates.m_blendState.m_targets[i] = targetBlendState;
                        }

                        shaderAssetCreator.SetRenderStates(renderStates);
                    }

                    Outcome<AZStd::string, AZStd::string> hlslSourceCodeOutcome = Utils::ReadFile(hlslFullPath, AZ::RPI::JsonUtils::AtomMaxFileSize);
                    if (!hlslSourceCodeOutcome.IsSuccess())
                    {
                        AZ_Error(
                            ShaderAssetBuilderName, false, "Failed to obtain shader source from %s. [%s]", hlslFullPath.c_str(),
                            hlslSourceCodeOutcome.GetError().c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    AZStd::string hlslSourceCode = hlslSourceCodeOutcome.TakeValue();

                    // The root ShaderVariantAsset needs to be created with the known uuid of the source .shader asset because
                    // the ShaderAsset owns a Data::Asset<> reference that gets serialized. It must have the correct uuid
                    // so the root ShaderVariantAsset is found when the ShaderAsset is deserialized.
                    uint32_t rootVariantProductSubId = RPI::ShaderAsset::MakeProductAssetSubId(
                        shaderPlatformInterface->GetAPIUniqueIndex(), supervariantIndex,
                        aznumeric_cast<uint32_t>(RPI::ShaderAssetSubId::RootShaderVariantAsset));
                    auto assetIdOutcome = RPI::AssetUtils::MakeAssetId(shaderFullPath, rootVariantProductSubId);
                    AZ_Assert(assetIdOutcome.IsSuccess(), "Failed to get AssetId from shader %s", shaderFullPath.c_str());
                    const Data::AssetId variantAssetId = assetIdOutcome.TakeValue();

                    RPI::ShaderVariantListSourceData::VariantInfo rootVariantInfo;
                    ShaderVariantCreationContext shaderVariantCreationContext = {
                        *shaderPlatformInterface,
                        request.m_platformInfo,
                        buildOptions.m_compilerArguments,
                        request.m_tempDirPath,
                        shaderAssetBuildTimestamp,
                        shaderSourceData,
                        *shaderOptionGroupLayout.get(),
                        shaderEntryPoints,
                        variantAssetId,
                        superVariantAzslinStemName,
                        hlslFullPath,
                        hlslSourceCode};


                    AZStd::optional<RHI::ShaderPlatformInterface::ByProducts> outputByproducts;
                    auto rootShaderVariantAssetOutcome = ShaderVariantAssetBuilder::CreateShaderVariantAsset(rootVariantInfo, shaderVariantCreationContext, outputByproducts);
                    if (!rootShaderVariantAssetOutcome.IsSuccess())
                    {
                        AZ_Error(ShaderAssetBuilderName, false, "%s\n", rootShaderVariantAssetOutcome.GetError().c_str())
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    Data::Asset<RPI::ShaderVariantAsset> rootShaderVariantAsset = rootShaderVariantAssetOutcome.TakeValue();

                    shaderAssetCreator.SetRootShaderVariantAsset(rootShaderVariantAsset);

                    if (!shaderAssetCreator.EndSupervariant())
                    {
                        AZ_Error(
                            ShaderAssetBuilderName, false, "Failed to create shader asset for supervariant [%s]", supervariantInfo.m_name.GetCStr())
                            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }

                    // Time to save the root variant related assets in the cache.
                    AssetBuilderSDK::JobProduct assetProduct;
                    if (!ShaderVariantAssetBuilder::SerializeOutShaderVariantAsset(
                            rootShaderVariantAsset, superVariantAzslinStemName, request.m_tempDirPath, *shaderPlatformInterface,
                            rootVariantProductSubId,
                            assetProduct))
                    {
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                        return;
                    }
                    response.m_outputProducts.push_back(assetProduct);

                    if (outputByproducts)
                    {
                        // add byproducts as job output products:
                        uint32_t subProductType = aznumeric_cast<uint32_t>(RPI::ShaderAssetSubId::FirstByProduct);
                        for (const AZStd::string& byproduct : outputByproducts.value().m_intermediatePaths)
                        {
                            AssetBuilderSDK::JobProduct jobProduct;
                            jobProduct.m_productFileName = byproduct;
                            jobProduct.m_productAssetType = Uuid::CreateName("DebugInfoByProduct-PdbOrDxilTxt");
                            jobProduct.m_productSubID = RPI::ShaderAsset::MakeProductAssetSubId(
                                shaderPlatformInterface->GetAPIUniqueIndex(), supervariantIndex,
                                subProductType++);
                            response.m_outputProducts.push_back(AZStd::move(jobProduct));
                        }
                    }


                    supervariantIndex++;

                } // end for the supervariant

                shaderAssetCreator.EndAPI();

            } // end for all ShaderPlatformInterfaces
            
            Data::Asset<RPI::ShaderAsset> shaderAsset;
            if (!shaderAssetCreator.End(shaderAsset))
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
            
            if (!SerializeOutShaderAsset(shaderAsset, request.m_tempDirPath, response))
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
            
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
                        
            AZ_TracePrintf(ShaderAssetBuilderName, "Finished processing %s in %.3f seconds\n", request.m_sourceFile.c_str(), timer.GetDeltaTimeInSeconds());
            
            ShaderBuilderUtility::LogProfilingData(ShaderAssetBuilderName, shaderFileName);
        }

    } // ShaderBuilder
} // AZ
