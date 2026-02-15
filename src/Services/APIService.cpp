#include "Services/APIService.h"
#include "Services/SourceScanner.h"
#include "Services/ContainerRegistry.h"
#include "Services/ContainerManager.h"
#include "Services/INISettings.h"
#include "Hooks/CraftingSession.h"
#include "Hooks/InventoryHooks.h"
#include "Version.h"

namespace Services {

    APIService* APIService::GetSingleton() {
        static APIService singleton;
        return &singleton;
    }

    std::int32_t APIService::GetAPIVersion() {
        return Version::MAJOR * 100 + Version::MINOR;
    }

    bool APIService::IsSessionActive() const {
        return Hooks::g_craftingSession.active;
    }

    void APIService::RefreshCache() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cacheValid = false;
        m_cache.clear();
        logger::debug("API: Cache invalidated");
    }

    void APIService::EnsureCacheValid() {
        // If crafting session is active, we use its cache directly
        if (Hooks::g_craftingSession.active) {
            return;
        }

        // Otherwise, build our own cache if needed
        if (m_cacheValid) {
            return;
        }

        m_cache.clear();

        // Get the original function pointer for inventory queries
        auto originalGetContainerItemCount = Hooks::InventoryHooks::GetOriginalGetContainerItemCount();
        if (!originalGetContainerItemCount) {
            logger::warn("API: Cannot build cache - hooks not installed");
            return;
        }

        // Use SourceScanner to build the cache
        ScanConfig config;
        config.stationType = StationType::Unknown;  // No filtering for API
        config.isCookingStation = false;
        config.includeGlobals = true;
        config.includeFollowers = true;
        config.maxDistance = 0.0f;  // Use INI setting

        auto result = SourceScanner::GetSingleton()->ScanSources(config, originalGetContainerItemCount);
        m_cache = std::move(result.inventoryCache);
        m_cacheValid = true;

        logger::debug("API: Cache built with {} unique items", static_cast<int>(m_cache.size()));
    }

    bool APIService::IsFormTypeAllowedForStation(RE::FormType a_formType, StationType a_station) const {
        // If no station filter, allow all
        if (a_station == StationType::Unknown) {
            return true;
        }

        // Use INISettings filtering logic
        return INISettings::GetSingleton()->ShouldPullFormType(a_station, a_formType);
    }

    std::int32_t APIService::GetCombinedItemCount(RE::TESBoundObject* a_item, std::int32_t a_stationType) {
        if (!a_item) return 0;

        std::lock_guard<std::mutex> lock(m_mutex);

        // If crafting session active, use its cache
        if (Hooks::g_craftingSession.active) {
            // Apply station filter if specified
            if (a_stationType >= 0 && a_stationType < static_cast<std::int32_t>(StationType::COUNT)) {
                auto station = static_cast<StationType>(a_stationType);
                if (!IsFormTypeAllowedForStation(a_item->GetFormType(), station)) {
                    return 0;
                }
            }
            return Hooks::g_craftingSession.GetCachedItemCount(a_item);
        }

        // Otherwise use our own cache
        EnsureCacheValid();

        // Apply station filter if specified
        if (a_stationType >= 0 && a_stationType < static_cast<std::int32_t>(StationType::COUNT)) {
            auto station = static_cast<StationType>(a_stationType);
            if (!IsFormTypeAllowedForStation(a_item->GetFormType(), station)) {
                return 0;
            }
        }

        auto it = m_cache.find(a_item);
        return it != m_cache.end() ? it->second : 0;
    }

    void APIService::GetAvailableItems(
        std::int32_t a_stationType,
        std::vector<RE::TESBoundObject*>& outItems,
        std::vector<std::int32_t>& outCounts)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        outItems.clear();
        outCounts.clear();

        // Determine station type filter
        StationType stationFilter = StationType::Unknown;
        if (a_stationType >= 0 && a_stationType < static_cast<std::int32_t>(StationType::COUNT)) {
            stationFilter = static_cast<StationType>(a_stationType);
        }

        // Get the cache to use
        const std::unordered_map<RE::TESBoundObject*, std::int32_t>* cache = nullptr;

        if (Hooks::g_craftingSession.active) {
            cache = &Hooks::g_craftingSession.inventoryCache;
        } else {
            EnsureCacheValid();
            cache = &m_cache;
        }

        // Build output arrays
        for (const auto& [item, count] : *cache) {
            if (!item || count <= 0) continue;

            // Apply station filter
            if (stationFilter != StationType::Unknown) {
                if (!IsFormTypeAllowedForStation(item->GetFormType(), stationFilter)) {
                    continue;
                }
            }

            outItems.push_back(item);
            outCounts.push_back(count);
        }

        logger::debug("API: GetAvailableItems returned {} items for station type {}",
            static_cast<int>(outItems.size()), a_stationType);
    }

    std::vector<RE::TESObjectREFR*> APIService::GetRegisteredContainers() const {
        std::vector<RE::TESObjectREFR*> result;

        auto* registry = ContainerRegistry::GetSingleton();
        auto* settings = INISettings::GetSingleton();

        // Get player-marked containers from registry
        auto overrides = registry->GetPlayerOverrides();
        std::unordered_set<RE::FormID> addedFormIDs;

        for (const auto& [formID, info] : overrides) {
            // Only include active containers (local or global state)
            if (info.state == ContainerState::kLocal ||
                info.state == ContainerState::kGlobal)
            {
                auto* form = RE::TESForm::LookupByID(formID);
                auto* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
                if (refr) {
                    result.push_back(refr);
                    addedFormIDs.insert(formID);
                }
            }
        }

        // Add global containers
        if (settings->GetEnableGlobalContainers()) {
            auto globalContainers = registry->GetGlobalContainers();
            for (auto* container : globalContainers) {
                if (container && !addedFormIDs.contains(container->GetFormID())) {
                    result.push_back(container);
                    addedFormIDs.insert(container->GetFormID());
                }
            }
        }

        return result;
    }

}  // namespace Services
