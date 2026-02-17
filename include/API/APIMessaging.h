#pragma once

#include <vector>

namespace API {

    /// SKSE message types for SCIE API
    /// External plugins can send/receive these via SKSE messaging interface
    enum class MessageType : std::uint32_t {
        // Requests (sent to SCIE)
        kRequestContainers = 'SCRC',      // Request registered containers
        kRequestContainerState = 'SCRS',  // Request state of specific container
        kRequestInventory = 'SCRI',       // Request merged inventory data

        // Responses (sent from SCIE)
        kResponseContainers = 'SCPC',     // Response with container refs
        kResponseContainerState = 'SCPS', // Response with container state
        kResponseInventory = 'SCPI'       // Response with inventory data
    };

    /// Request structure for container state query
    struct ContainerStateRequest {
        RE::FormID containerFormID;
    };

    /// Response structure for container state query
    struct ContainerStateResponse {
        RE::FormID containerFormID;
        std::int32_t state;  // 0=off, 1=local, 2=global
        bool found;
    };

    /// Request structure for inventory query
    struct InventoryRequest {
        std::int32_t stationType;  // -1 = all, 0-3 = specific station
    };

    /// Response structure for inventory query
    struct InventoryResponse {
        std::int32_t itemCount;         // Number of unique items
        std::int32_t activeStationType; // -1 = no session, 0-3 = station type
    };

    /// SCIE API message handler
    /// Register this with SKSE messaging to receive API responses
    class APIMessaging {
    public:
        static APIMessaging* GetSingleton();

        /// Initialize API messaging (register handlers)
        void Initialize();

        /// Process incoming SKSE messages
        void HandleMessage(SKSE::MessagingInterface::Message* a_msg);

        /// Get plugin name for SKSE messaging
        static constexpr const char* GetPluginName() { return "SCIE"; }

        /// Get registered containers as refs
        /// Note: This queries the registry directly, no messaging needed
        std::vector<RE::TESObjectREFR*> GetRegisteredContainers() const;

        /// Get state of a specific container
        /// Returns: 0=off, 1=local, 2=global
        std::int32_t GetContainerState(RE::TESObjectREFR* a_container) const;

    private:
        APIMessaging() = default;
        ~APIMessaging() = default;
        APIMessaging(const APIMessaging&) = delete;
        APIMessaging& operator=(const APIMessaging&) = delete;
    };

}  // namespace API
