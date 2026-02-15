#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "Services/INISettings.h"
#include "Hooks/CraftingSession.h"

namespace Services {

    /// Function pointer type for GetContainerItemCount (matches InventoryHooks.h)
    using GetContainerItemCount_t = std::int32_t(*)(RE::TESObjectREFR*, bool, bool);

    /// Configuration for source scanning
    struct ScanConfig {
        float maxDistance = 0.0f;           // 0 = use INI setting
        bool includeGlobals = true;         // Include global containers
        bool includeFollowers = true;       // Include followers/spouses
        StationType stationType = StationType::Unknown;  // For filtering
        bool isCookingStation = false;      // For follower ALCH exception
    };

    /// Result of a source scan
    struct ScanResult {
        std::vector<Hooks::ContainerSource> sources;
        std::unordered_map<RE::TESBoundObject*, std::int32_t> inventoryCache;
        std::size_t favoritedItemsExcluded = 0;
    };

    /// Service for scanning and collecting crafting sources
    /// Extracts the source scanning logic from InventoryHooks for reuse by the public API
    class SourceScanner {
    public:
        static SourceScanner* GetSingleton();

        /// Scan all available sources and build inventory cache
        /// This is the main entry point for gathering crafting sources
        /// @param config Scanning configuration
        /// @param originalGetContainerItemCount Function pointer to original GetContainerItemCount
        /// @return ScanResult containing sources, inventory cache, and stats
        ScanResult ScanSources(
            const ScanConfig& config,
            GetContainerItemCount_t originalGetContainerItemCount);

        /// Get all source references without building full inventory cache
        /// Lighter-weight query for just listing containers
        /// @param config Scanning configuration
        /// @return Vector of container references
        std::vector<RE::TESObjectREFR*> GetAllSourceRefs(const ScanConfig& config);

        /// Rebuild inventory cache from existing sources
        /// Called after crafting to pick up newly created items
        /// @param sources The sources to scan
        /// @param isCookingStation Whether current station is a cooking station
        /// @param outCache Output map for item counts
        /// @return Number of favorited items excluded
        std::size_t RebuildInventoryCache(
            const std::vector<Hooks::ContainerSource>& sources,
            bool isCookingStation,
            std::unordered_map<RE::TESBoundObject*, std::int32_t>& outCache);

    private:
        SourceScanner() = default;
        ~SourceScanner() = default;
        SourceScanner(const SourceScanner&) = delete;
        SourceScanner& operator=(const SourceScanner&) = delete;

        /// Build set of favorited items from player inventory
        std::unordered_set<RE::TESBoundObject*> GetFavoritedItems();

        mutable std::mutex m_mutex;
    };

}  // namespace Services
