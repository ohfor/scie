#include "API/APIMessaging.h"
#include "Services/ContainerRegistry.h"
#include "Services/ContainerManager.h"
#include "Services/INISettings.h"
#include "Hooks/CraftingSession.h"

namespace API {

    APIMessaging* APIMessaging::GetSingleton() {
        static APIMessaging singleton;
        return &singleton;
    }

    void APIMessaging::Initialize() {
        logger::info("SCIE API messaging initialized");
    }

    void APIMessaging::HandleMessage(SKSE::MessagingInterface::Message* a_msg) {
        if (!a_msg) return;

        // Handle SKSE messages from other plugins requesting SCIE data
        auto type = static_cast<MessageType>(a_msg->type);

        switch (type) {
            case MessageType::kRequestContainers: {
                logger::debug("API: Received container list request");
                // External plugin is requesting our container list
                // They would need to provide a callback mechanism - for now we just log
                break;
            }

            case MessageType::kRequestContainerState: {
                if (a_msg->data && a_msg->dataLen >= sizeof(ContainerStateRequest)) {
                    auto* request = static_cast<ContainerStateRequest*>(a_msg->data);
                    logger::debug("API: Received container state request for {:08X}",
                        request->containerFormID);
                }
                break;
            }

            case MessageType::kRequestInventory: {
                if (a_msg->data && a_msg->dataLen >= sizeof(InventoryRequest)) {
                    auto* request = static_cast<InventoryRequest*>(a_msg->data);
                    logger::debug("API: Received inventory request for station type {}",
                        request->stationType);
                }
                break;
            }

            default:
                // Unknown message type, ignore
                break;
        }
    }

    std::vector<RE::TESObjectREFR*> APIMessaging::GetRegisteredContainers() const {
        std::vector<RE::TESObjectREFR*> result;

        auto* registry = Services::ContainerRegistry::GetSingleton();
        auto* settings = Services::INISettings::GetSingleton();

        // Get player-marked containers from registry
        auto overrides = registry->GetPlayerOverrides();
        for (const auto& [formID, info] : overrides) {
            // Only include active containers (local or global state)
            if (info.state == Services::ContainerState::kLocal ||
                info.state == Services::ContainerState::kGlobal)
            {
                auto* form = RE::TESForm::LookupByID(formID);
                auto* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
                if (refr) {
                    result.push_back(refr);
                }
            }
        }

        // Add INI-configured containers that are enabled
        if (settings->GetEnableINIContainers()) {
            float maxDistance = settings->GetMaxContainerDistance();
            auto nearbyContainers = Services::ContainerManager::GetSingleton()->GetNearbyCraftingContainers(maxDistance);
            for (auto* container : nearbyContainers) {
                if (container) {
                    // Check if not already in result
                    bool found = false;
                    for (auto* existing : result) {
                        if (existing && existing->GetFormID() == container->GetFormID()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        result.push_back(container);
                    }
                }
            }
        }

        // Add global containers
        if (settings->GetEnableGlobalContainers()) {
            auto globalContainers = registry->GetGlobalContainers();
            for (auto* container : globalContainers) {
                if (container) {
                    // Check if not already in result
                    bool found = false;
                    for (auto* existing : result) {
                        if (existing && existing->GetFormID() == container->GetFormID()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        result.push_back(container);
                    }
                }
            }
        }

        return result;
    }

    std::int32_t APIMessaging::GetContainerState(RE::TESObjectREFR* a_container) const {
        if (!a_container) return 0;
        return Services::ContainerRegistry::GetSingleton()->GetContainerState(a_container->GetFormID());
    }

}  // namespace API
