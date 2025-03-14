/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/AzCoreModule.h>

// Component includes
#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/IO/Streamer/StreamerComponent.h>
#include <AzCore/Jobs/JobManagerComponent.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Memory/MemoryComponent.h>
#include <AzCore/Script/ScriptSystemComponent.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Slice/SliceSystemComponent.h>
#include <AzCore/Slice/SliceMetadataInfoComponent.h>
#include <AzCore/UserSettings/UserSettingsComponent.h>
#include <AzCore/Time/TimeSystemComponent.h>
#include <AzCore/Console/LoggerSystemComponent.h>
#include <AzCore/EBus/EventSchedulerSystemComponent.h>

namespace AZ
{
    AzCoreModule::AzCoreModule()
        : AZ::Module()
    {
        m_descriptors.insert(m_descriptors.end(), {
            MemoryComponent::CreateDescriptor(),
            StreamerComponent::CreateDescriptor(),
            JobManagerComponent::CreateDescriptor(),
            JsonSystemComponent::CreateDescriptor(),
            AssetManagerComponent::CreateDescriptor(),
            UserSettingsComponent::CreateDescriptor(),
            SliceComponent::CreateDescriptor(),
            SliceSystemComponent::CreateDescriptor(),
            SliceMetadataInfoComponent::CreateDescriptor(),
            TimeSystemComponent::CreateDescriptor(),
            LoggerSystemComponent::CreateDescriptor(),
            EventSchedulerSystemComponent::CreateDescriptor(),

#if !defined(AZCORE_EXCLUDE_LUA)
            ScriptSystemComponent::CreateDescriptor(),
#endif // #if !defined(AZCORE_EXCLUDE_LUA)
        });
    }

    AZ::ComponentTypeList AzCoreModule::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList
        {
            azrtti_typeid<TimeSystemComponent>(),
            azrtti_typeid<LoggerSystemComponent>(),
            azrtti_typeid<EventSchedulerSystemComponent>(),
        };
    }
}
