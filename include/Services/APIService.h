#pragma once

#include <vector>
#include <unordered_map>
#include <mutex>
#include "Services/INISettings.h"

namespace Services {

    /// Public API service for external mod queries
    /// Provides access to SCIE's merged inventory without requiring a crafting session
    class APIService {
    public:
        static APIService* GetSingleton();

        /// Get combined item count from player + all active containers
        /// @param a_item The item to count
        /// @param a_stationType Station type filter (-1 = no filter, 0-3 = specific)
        /// @return Total count across all sources
        std::int32_t GetCombinedItemCount(RE::TESBoundObject* a_item, std::int32_t a_stationType = -1);

        /// Get all available items from merged inventory
        /// @param a_stationType Station type filter (-1 = no filter, 0-3 = specific)
        /// @param outItems Output vector for item forms
        /// @param outCounts Output vector for counts (parallel with outItems)
        void GetAvailableItems(
            std::int32_t a_stationType,
            std::vector<RE::TESBoundObject*>& outItems,
            std::vector<std::int32_t>& outCounts);

        /// Force refresh of the API cache
        /// Call this if container contents have changed outside of crafting
        void RefreshCache();

        /// Check if a crafting session is currently active
        bool IsSessionActive() const;

        /// Get all registered containers as refs
        /// @param a_globalOnly If true (default), only return global containers — stable
        ///        regardless of player location. If false, also include local containers
        ///        that are currently loaded (cell-dependent, may vary between calls).
        std::vector<RE::TESObjectREFR*> GetRegisteredContainers(bool a_globalOnly = true) const;

        /// Get API version (major * 100 + minor, e.g., 254 = v2.5.4)
        static std::int32_t GetAPIVersion();

    private:
        APIService() = default;
        ~APIService() = default;
        APIService(const APIService&) = delete;
        APIService& operator=(const APIService&) = delete;

        /// Build or refresh the inventory cache
        void EnsureCacheValid();

        /// Check if form type is allowed for given station type
        bool IsFormTypeAllowedForStation(RE::FormType a_formType, StationType a_station) const;

        /// Cached inventory data (used when no crafting session active)
        std::unordered_map<RE::TESBoundObject*, std::int32_t> m_cache;
        bool m_cacheValid = false;
        mutable std::mutex m_mutex;
    };

}  // namespace Services
