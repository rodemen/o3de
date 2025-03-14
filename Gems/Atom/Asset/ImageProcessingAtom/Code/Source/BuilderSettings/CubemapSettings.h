/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/Memory.h>
#include <BuilderSettings/ImageProcessingDefines.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace ImageProcessingAtom
{
    //! settings related to cubemap. Part of texture preset setting. only useful when cubemap enabled
    struct CubemapSettings
    {
        AZ_TYPE_INFO(CubemapSettings, "{A4046BCB-E42E-4C77-AF77-1A1AD9B7CC64}");
        AZ_CLASS_ALLOCATOR(CubemapSettings, AZ::SystemAllocator, 0);
        bool operator!=(const CubemapSettings& other);
        bool operator==(const CubemapSettings& other);
        static void Reflect(AZ::ReflectContext* context);

        // "cm_ftype", cubemap angular filter type: gaussian, cone, disc, cosine, cosine_power, ggx
        CubemapFilterType m_filter;

        // "cm_fangle", base filter angle for cubemap filtering(degrees), 0 - disabled
        float m_angle;

        // "cm_fmipangle", initial mip filter angle for cubemap filtering(degrees), 0 - disabled
        float m_mipAngle;

        // "cm_fmipslope", mip filter angle multiplier for cubemap filtering, 1 - default"
        float m_mipSlope;

        // "cm_edgefixup", cubemap edge fix-up width, 0 - disabled
        float m_edgeFixup;

        // generate an IBL specular cubemap
        bool m_generateIBLSpecular = false;

        // the UUID of the preset to be used for generating the IBL specular cubemap
        AZ::Uuid m_iblSpecularPreset = AZ::Uuid::CreateNull();

        // generate an IBL diffuse cubemap
        bool m_generateIBLDiffuse = false;

        // the UUID of the preset to be used for generating the IBL diffuse cubemap
        AZ::Uuid m_iblDiffusePreset = AZ::Uuid::CreateNull();

        // "cm_requiresconvolve", convolve the cubemap mips
        bool m_requiresConvolve = true;

        //product subId, allows a specific subId to be specified for an output cubemap product
        AZ::u32 m_subId = AZ::RPI::StreamingImageAsset::GetImageAssetSubId();
    };
} // namespace ImageProcessingAtom
