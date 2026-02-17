#include "API/APIMessaging.h"
#include "Services/APIService.h"
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
        const char* sender = a_msg->sender;

        // Need sender to dispatch response
        if (!sender || sender[0] == '\0') {
            logger::warn("API: Received message with no sender, cannot respond");
            return;
        }

        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) {
            logger::error("API: SKSE messaging interface unavailable");
            return;
        }

        switch (type) {
            case MessageType::kRequestContainers: {
                logger::debug("API: Received container list request from {}", sender);

                // Get registered containers and extract FormIDs
                auto containers = GetRegisteredContainers();
                std::vector<RE::FormID> formIDs;
                formIDs.reserve(containers.size());
                for (auto* container : containers) {
                    if (container) {
                        formIDs.push_back(container->GetFormID());
                    }
                }

                // Dispatch response with FormID array
                messaging->Dispatch(
                    static_cast<std::uint32_t>(MessageType::kResponseContainers),
                    formIDs.data(),
                    static_cast<std::uint32_t>(formIDs.size() * sizeof(RE::FormID)),
                    sender
                );

                logger::debug("API: Sent {} container FormIDs to {}", formIDs.size(), sender);
                break;
            }

            case MessageType::kRequestContainerState: {
                if (a_msg->data && a_msg->dataLen >= sizeof(ContainerStateRequest)) {
                    auto* request = static_cast<ContainerStateRequest*>(a_msg->data);
                    logger::debug("API: Received container state request for {:08X} from {}",
                        request->containerFormID, sender);

                    // Look up container and get state
                    ContainerStateResponse response;
                    response.containerFormID = request->containerFormID;
                    response.found = false;
                    response.state = 0;

                    auto* form = RE::TESForm::LookupByID(request->containerFormID);
                    auto* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
                    if (refr) {
                        response.state = GetContainerState(refr);
                        response.found = true;
                    }

                    messaging->Dispatch(
                        static_cast<std::uint32_t>(MessageType::kResponseContainerState),
                        &response,
                        sizeof(response),
                        sender
                    );

                    logger::debug("API: Sent container state {} (found={}) to {}",
                        response.state, response.found, sender);
                }
                break;
            }

            case MessageType::kRequestInventory: {
                if (a_msg->data && a_msg->dataLen >= sizeof(InventoryRequest)) {
                    auto* request = static_cast<InventoryRequest*>(a_msg->data);
                    logger::debug("API: Received inventory request for station type {} from {}",
                        request->stationType, sender);

                    // Build response with summary info
                    InventoryResponse response;
                    auto* sessionMgr = Hooks::CraftingSessionManager::GetSingleton();
                    if (sessionMgr->IsSessionActive()) {
                        response.activeStationType = static_cast<std::int32_t>(
                            sessionMgr->GetSession().stationType);
                    } else {
                        response.activeStationType = -1;
                    }

                    // Get item count from APIService
                    auto* apiService = Services::APIService::GetSingleton();
                    std::vector<RE::TESBoundObject*> items;
                    std::vector<std::int32_t> counts;
                    apiService->GetAvailableItems(request->stationType, items, counts);
                    response.itemCount = static_cast<std::int32_t>(items.size());

                    messaging->Dispatch(
                        static_cast<std::uint32_t>(MessageType::kResponseInventory),
                        &response,
                        sizeof(response),
                        sender
                    );

                    logger::debug("API: Sent inventory summary ({} items, stationType={}) to {}",
                        response.itemCount, response.activeStationType, sender);
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
