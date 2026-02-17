#pragma once

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace Services {

    /// Integration service for SLID (Skyrim Linked Item Distribution)
    /// Handles SLID detection, network discovery, and container queries
    class SLIDIntegration {
    public:
        static SLIDIntegration* GetSingleton();

        /// Initialize the service - register SKSE message listener
        void Initialize();

        /// Check if SLID is installed (DLL present)
        bool IsSLIDInstalled() const;

        /// Request network list from SLID via SKSE messaging
        /// Results arrive asynchronously via HandleMessage
        void RequestNetworkList();

        /// Request containers for a specific network
        /// Results arrive asynchronously via HandleMessage
        void RequestNetworkContainers(const std::string& a_networkName);

        /// Handle incoming SKSE messages from SLID
        void HandleMessage(SKSE::MessagingInterface::Message* a_msg);

        /// Get cached network names (from last RequestNetworkList)
        std::vector<std::string> GetNetworkNames() const;

        /// Check if a network is enabled by the user
        bool IsNetworkEnabled(const std::string& a_networkName) const;

        /// Enable/disable a network
        void SetNetworkEnabled(const std::string& a_networkName, bool a_enabled);

        /// Get all enabled network names
        std::vector<std::string> GetEnabledNetworks() const;

        /// Get collected container FormIDs (populated by RequestNetworkContainers responses)
        std::vector<RE::FormID> GetCollectedContainers() const;

        /// Get container refs for all enabled networks (resolved from FormIDs)
        /// Logs debug warnings for any missing networks
        std::vector<RE::TESObjectREFR*> GetEnabledNetworkContainerRefs();

        /// Check if any enabled network is missing (was deleted in SLID)
        std::vector<std::string> GetMissingNetworks() const;

        /// Remove a network from enabled set (for cleaning up missing networks)
        void RemoveNetwork(const std::string& a_networkName);

        /// Request containers for all enabled networks and collect FormIDs
        /// Returns count of networks queried (excludes missing)
        std::uint32_t QueryEnabledNetworkContainers();

        /// Clear cached data (call on game load)
        void ClearCache();

        /// Refresh: request network list and containers for enabled networks
        void Refresh();

        // Cosave persistence
        void SaveToCoSave(SKSE::SerializationInterface* a_intfc);
        void LoadFromCoSave(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version);
        void Revert();

    private:
        SLIDIntegration() = default;
        ~SLIDIntegration() = default;
        SLIDIntegration(const SLIDIntegration&) = delete;
        SLIDIntegration& operator=(const SLIDIntegration&) = delete;

        /// Cached network names from SLID
        std::vector<std::string> m_networkNames;
        mutable std::mutex m_networkNamesMutex;

        /// User-enabled networks (persisted to cosave)
        std::unordered_set<std::string> m_enabledNetworks;
        mutable std::mutex m_enabledNetworksMutex;

        /// Collected container FormIDs from all responses (flat pool, deduplicated)
        std::vector<RE::FormID> m_collectedContainers;
        std::unordered_set<RE::FormID> m_seenContainers;  // For deduplication
        mutable std::mutex m_containersMutex;

        /// Track if SLID was detected
        mutable bool m_slidDetected = false;
        mutable bool m_slidChecked = false;
    };

}  // namespace Services
