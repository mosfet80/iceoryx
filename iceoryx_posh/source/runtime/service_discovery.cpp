// Copyright (c) 2019 - 2021 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_posh/runtime/service_discovery.hpp"

namespace iox
{
namespace runtime
{
ServiceContainer ServiceDiscovery::findService(const cxx::optional<capro::IdString_t>& service,
                                               const cxx::optional<capro::IdString_t>& instance,
                                               const cxx::optional<capro::IdString_t>& event) noexcept
{
    ServiceContainer searchResult;

    auto lambda = [&](const capro::ServiceDescription& entry) { searchResult.emplace_back(entry); };
    findService(service, instance, event, lambda);

    return searchResult;
}

void ServiceDiscovery::findService(
    const cxx::optional<capro::IdString_t>& service,
    const cxx::optional<capro::IdString_t>& instance,
    const cxx::optional<capro::IdString_t>& event,
    const cxx::function_ref<void(const capro::ServiceDescription&)>& callableForEach) noexcept
{
    if (!callableForEach)
    {
        return;
    }

    m_serviceRegistrySubscriber.take().and_then([&](popo::Sample<const roudi::ServiceRegistry>& serviceRegistrySample) {
        m_serviceRegistry = *serviceRegistrySample;
    });

    m_serviceRegistry.find(
        service, instance, event, [&](const roudi::ServiceRegistry::ServiceDescriptionEntry& serviceEntry) {
            if (serviceEntry.publisherCount > 0)
            {
                callableForEach(serviceEntry.serviceDescription);
            }
        });
}

void ServiceDiscovery::enableEvent(popo::TriggerHandle&& triggerHandle, const ServiceDiscoveryEvent event) noexcept
{
    switch (event)
    {
    case ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED:
    {
        m_serviceRegistrySubscriber.enableEvent(std::move(triggerHandle), popo::SubscriberEvent::DATA_RECEIVED);
        break;
    }
    default:
    {
        LogWarn() << "ServiceDiscovery::enableEvent() called with unkown event!";
        errorHandler(Error::kPOSH__SERVICE_DISCOVERY_UNKNOWN_EVENT_PROVIDED, nullptr, ErrorLevel::MODERATE);
    }
    }
}

void ServiceDiscovery::disableEvent(const ServiceDiscoveryEvent event) noexcept
{
    switch (event)
    {
    case ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED:
    {
        m_serviceRegistrySubscriber.disableEvent(popo::SubscriberEvent::DATA_RECEIVED);
        break;
    }
    default:
    {
        LogWarn() << "ServiceDiscovery::disableEvent() called with unkown event!";
        errorHandler(Error::kPOSH__SERVICE_DISCOVERY_UNKNOWN_EVENT_PROVIDED, nullptr, ErrorLevel::MODERATE);
    }
    }
}

void ServiceDiscovery::invalidateTrigger(const uint64_t uniqueTriggerId)
{
    m_serviceRegistrySubscriber.invalidateTrigger(uniqueTriggerId);
}

popo::WaitSetIsConditionSatisfiedCallback
ServiceDiscovery::getCallbackForIsStateConditionSatisfied(const popo::SubscriberState state)
{
    return m_serviceRegistrySubscriber.getCallbackForIsStateConditionSatisfied(state);
}

} // namespace runtime
} // namespace iox