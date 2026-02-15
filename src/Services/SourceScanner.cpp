#include "Services/SourceScanner.h"
#include "Services/ContainerManager.h"
#include "Services/ContainerRegistry.h"
#include "Services/INISettings.h"

#include <chrono>
#include <map>

namespace Services {

    SourceScanner* SourceScanner::GetSingleton() {
        static SourceScanner singleton;
        return &singleton;
    }

    std::unordered_set<RE::TESBoundObject*> SourceScanner::GetFavoritedItems() {
        std::unordered_set<RE::TESBoundObject*> favoritedItems;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            auto playerInventory = player->GetInventory();
            for (auto& [item, data] : playerInventory) {
                if (item && data.second && data.second->IsFavorited()) {
                    favoritedItems.insert(item);
                }
            }
        }
        return favoritedItems;
    }

    ScanResult SourceScanner::ScanSources(
        const ScanConfig& config,
        GetContainerItemCount_t originalGetContainerItemCount)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        ScanResult result;
        auto scanStartTime = std::chrono::high_resolution_clock::now();

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("SourceScanner::ScanSources: Player not available");
            return result;
        }

        auto* containerMgr = ContainerManager::GetSingleton();
        auto* registry = ContainerRegistry::GetSingleton();
        auto* settings = INISettings::GetSingleton();

        // Add player as first source
        auto t1 = std::chrono::high_resolution_clock::now();
        std::int32_t playerCount = originalGetContainerItemCount(player, false, true);
        auto t2 = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] Player inventory count took {:.2f}ms",
            std::chrono::duration<double, std::milli>(t2 - t1).count());

        result.sources.push_back({
            player->CreateRefHandle(),
            playerCount,
            0  // Player starts at index 0
        });
        logger::info("Scan source: Player ({} items)", playerCount);

        // Track current index for virtual inventory
        std::int32_t currentIndex = playerCount;

        // Helper to check if a container is already in sources
        auto isAlreadyInSources = [&result](RE::FormID formID) {
            for (const auto& source : result.sources) {
                auto* ref = source.ref.get().get();
                if (ref && ref->GetFormID() == formID) {
                    return true;
                }
            }
            return false;
        };

        // Add nearby containers
        float maxDistance = config.maxDistance > 0 ? config.maxDistance : settings->GetMaxContainerDistance();
        auto t3 = std::chrono::high_resolution_clock::now();
        auto nearbyContainers = containerMgr->GetNearbyCraftingContainers(maxDistance);
        auto t4 = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] GetNearbyCraftingContainers took {:.2f}ms ({} containers)",
            std::chrono::duration<double, std::milli>(t4 - t3).count(),
            static_cast<int>(nearbyContainers.size()));

        auto t5 = std::chrono::high_resolution_clock::now();
        for (auto* container : nearbyContainers) {
            if (!container) continue;

            std::int32_t count = originalGetContainerItemCount(container, false, true);
            if (count > 0) {
                result.sources.push_back({
                    container->CreateRefHandle(),
                    count,
                    currentIndex
                });

                auto baseName = container->GetBaseObject() ? container->GetBaseObject()->GetName() : "Unknown";
                logger::info("Scan source: {} ({:08X}) - {} items at index {}",
                    baseName, static_cast<std::uint32_t>(container->GetFormID()), count, currentIndex);

                currentIndex += count;
            }
        }
        auto t6 = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] Container count loop took {:.2f}ms",
            std::chrono::duration<double, std::milli>(t6 - t5).count());

        // Add global containers (accessible from anywhere)
        if (config.includeGlobals && settings->GetEnableGlobalContainers()) {
            auto t6b = std::chrono::high_resolution_clock::now();
            auto globalContainers = registry->GetGlobalContainers();

            logger::info("Global containers query returned {} containers",
                static_cast<int>(globalContainers.size()));

            for (auto* container : globalContainers) {
                if (!container) continue;

                // Skip if already added as a local container (prevents double-counting)
                if (isAlreadyInSources(container->GetFormID())) {
                    auto baseName = container->GetBaseObject() ? container->GetBaseObject()->GetName() : "Unknown";
                    logger::debug("Global container {} ({:08X}) already in sources, skipping",
                        baseName, static_cast<std::uint32_t>(container->GetFormID()));
                    continue;
                }

                std::int32_t count = originalGetContainerItemCount(container, false, true);
                if (count > 0) {
                    result.sources.push_back({
                        container->CreateRefHandle(),
                        count,
                        currentIndex
                    });

                    auto baseName = container->GetBaseObject() ? container->GetBaseObject()->GetName() : "Unknown";
                    logger::info("Scan source (GLOBAL): {} ({:08X}) - {} items at index {}",
                        baseName, static_cast<std::uint32_t>(container->GetFormID()), count, currentIndex);

                    currentIndex += count;
                }
            }
            auto t6c = std::chrono::high_resolution_clock::now();
            if (!globalContainers.empty()) {
                logger::debug("[PERF] Global containers took {:.2f}ms ({} containers)",
                    std::chrono::duration<double, std::milli>(t6c - t6b).count(),
                    static_cast<int>(globalContainers.size()));
            }
        }

        // Add nearby followers/spouses as sources
        if (config.includeFollowers && settings->GetEnableFollowerInventory()) {
            auto t6d = std::chrono::high_resolution_clock::now();
            auto nearbyFollowers = containerMgr->GetNearbyFollowers(maxDistance);

            for (auto* follower : nearbyFollowers) {
                if (!follower) continue;

                // Skip if already in sources (shouldn't happen, but safety check)
                if (isAlreadyInSources(follower->GetFormID())) continue;

                std::int32_t count = originalGetContainerItemCount(follower, false, true);
                if (count > 0) {
                    result.sources.push_back({
                        follower->CreateRefHandle(),
                        count,
                        currentIndex,
                        true  // isFollower
                    });

                    auto displayName = follower->GetDisplayFullName() ? follower->GetDisplayFullName() : "Unknown";
                    logger::info("Scan source (FOLLOWER): {} ({:08X}) - {} items at index {}",
                        displayName, static_cast<std::uint32_t>(follower->GetFormID()), count, currentIndex);

                    currentIndex += count;

                    // Check for NFF Additional Inventory container
                    auto* followerActor = follower->As<RE::Actor>();
                    if (followerActor) {
                        auto* nffContainer = containerMgr->GetNFFAdditionalInventory(followerActor);
                        if (nffContainer && !isAlreadyInSources(nffContainer->GetFormID())) {
                            std::int32_t nffCount = originalGetContainerItemCount(nffContainer, false, true);
                            if (nffCount > 0) {
                                result.sources.push_back({
                                    nffContainer->CreateRefHandle(),
                                    nffCount,
                                    currentIndex,
                                    true  // isFollower - same safety filter
                                });

                                auto nffDisplayName = follower->GetDisplayFullName() ? follower->GetDisplayFullName() : "Unknown";
                                logger::info("Scan source (NFF ADDITIONAL): {}'s satchel ({:08X}) - {} items at index {}",
                                    nffDisplayName, static_cast<std::uint32_t>(nffContainer->GetFormID()), nffCount, currentIndex);

                                currentIndex += nffCount;
                            }
                        }
                    }
                }
            }
            auto t6e = std::chrono::high_resolution_clock::now();
            logger::debug("[PERF] Follower scan took {:.2f}ms",
                std::chrono::duration<double, std::milli>(t6e - t6d).count());
        }

        // Build inventory cache - snapshot all item counts once
        auto t7 = std::chrono::high_resolution_clock::now();
        auto favoritedItems = GetFavoritedItems();
        if (!favoritedItems.empty()) {
            logger::info("Respecting {} favorited items (Essential Favorites compatibility)",
                static_cast<int>(favoritedItems.size()));
        }

        for (auto& source : result.sources) {
            auto* container = source.ref.get().get();
            if (!container) continue;

            auto inventory = container->GetInventory();
            for (auto& [item, data] : inventory) {
                if (item && data.first > 0) {
                    // Skip favorited items
                    if (favoritedItems.contains(item)) {
                        continue;
                    }
                    // Apply follower safety filter
                    if (source.isFollower && !Hooks::IsFollowerSafeFormType(item->GetFormType(), config.isCookingStation)) {
                        continue;
                    }
                    result.inventoryCache[item] += data.first;

                    // Per-source item breakdown for debugging
                    auto sourceName = container->IsPlayerRef() ? "Player"
                        : (container->GetDisplayFullName() ? container->GetDisplayFullName() : "Unknown");
                    logger::trace("  Cache: {} x{} from {} ({:08X}){}",
                        item->GetName(), data.first,
                        sourceName,
                        static_cast<std::uint32_t>(container->GetFormID()),
                        source.isFollower ? " [follower]" : "");
                }
            }
        }
        result.favoritedItemsExcluded = favoritedItems.size();
        auto t8 = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] Inventory cache build took {:.2f}ms ({} unique items, {} favorited skipped)",
            std::chrono::duration<double, std::milli>(t8 - t7).count(),
            static_cast<int>(result.inventoryCache.size()),
            static_cast<int>(favoritedItems.size()));

        auto scanEndTime = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] === SOURCE SCAN TOTAL: {:.2f}ms ===",
            std::chrono::duration<double, std::milli>(scanEndTime - scanStartTime).count());

        logger::info("Source scan complete: {} sources, {} total items, {} unique cached",
            static_cast<int>(result.sources.size()), currentIndex,
            static_cast<int>(result.inventoryCache.size()));

        // Log summary of cached items by form type for debugging station-specific issues
        std::map<RE::FormType, std::pair<int, int>> formTypeSummary;  // type -> (count of types, total quantity)
        for (const auto& [item, count] : result.inventoryCache) {
            if (item) {
                auto ft = item->GetFormType();
                formTypeSummary[ft].first++;
                formTypeSummary[ft].second += count;
            }
        }
        for (const auto& [ft, summary] : formTypeSummary) {
            logger::info("  Cache: {} {} items ({} total quantity)",
                summary.first,
                ft == RE::FormType::Misc ? "MISC" :
                ft == RE::FormType::Ingredient ? "INGR" :
                ft == RE::FormType::AlchemyItem ? "ALCH" :
                ft == RE::FormType::Weapon ? "WEAP" :
                ft == RE::FormType::Armor ? "ARMO" :
                ft == RE::FormType::SoulGem ? "SLGM" :
                ft == RE::FormType::Ammo ? "AMMO" : "OTHER",
                summary.second);
        }

        return result;
    }

    std::vector<RE::TESObjectREFR*> SourceScanner::GetAllSourceRefs(const ScanConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<RE::TESObjectREFR*> refs;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return refs;

        auto* containerMgr = ContainerManager::GetSingleton();
        auto* registry = ContainerRegistry::GetSingleton();
        auto* settings = INISettings::GetSingleton();

        // Add player
        refs.push_back(player);

        // Track FormIDs to prevent duplicates
        std::unordered_set<RE::FormID> addedFormIDs;
        addedFormIDs.insert(player->GetFormID());

        // Add nearby containers
        float maxDistance = config.maxDistance > 0 ? config.maxDistance : settings->GetMaxContainerDistance();
        auto nearbyContainers = containerMgr->GetNearbyCraftingContainers(maxDistance);
        for (auto* container : nearbyContainers) {
            if (container && !addedFormIDs.contains(container->GetFormID())) {
                refs.push_back(container);
                addedFormIDs.insert(container->GetFormID());
            }
        }

        // Add global containers
        if (config.includeGlobals && settings->GetEnableGlobalContainers()) {
            auto globalContainers = registry->GetGlobalContainers();
            for (auto* container : globalContainers) {
                if (container && !addedFormIDs.contains(container->GetFormID())) {
                    refs.push_back(container);
                    addedFormIDs.insert(container->GetFormID());
                }
            }
        }

        // Add followers
        if (config.includeFollowers && settings->GetEnableFollowerInventory()) {
            auto nearbyFollowers = containerMgr->GetNearbyFollowers(maxDistance);
            for (auto* follower : nearbyFollowers) {
                if (follower && !addedFormIDs.contains(follower->GetFormID())) {
                    refs.push_back(follower);
                    addedFormIDs.insert(follower->GetFormID());

                    // Check for NFF Additional Inventory
                    auto* followerActor = follower->As<RE::Actor>();
                    if (followerActor) {
                        auto* nffContainer = containerMgr->GetNFFAdditionalInventory(followerActor);
                        if (nffContainer && !addedFormIDs.contains(nffContainer->GetFormID())) {
                            refs.push_back(nffContainer);
                            addedFormIDs.insert(nffContainer->GetFormID());
                        }
                    }
                }
            }
        }

        return refs;
    }

    std::size_t SourceScanner::RebuildInventoryCache(
        const std::vector<Hooks::ContainerSource>& sources,
        bool isCookingStation,
        std::unordered_map<RE::TESBoundObject*, std::int32_t>& outCache)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        outCache.clear();
        auto favoritedItems = GetFavoritedItems();

        for (const auto& source : sources) {
            auto* container = source.ref.get().get();
            if (!container) continue;

            auto inventory = container->GetInventory();
            for (auto& [item, data] : inventory) {
                if (item && data.first > 0) {
                    // Skip favorited items
                    if (favoritedItems.contains(item)) {
                        continue;
                    }
                    // Apply follower safety filter
                    if (source.isFollower && !Hooks::IsFollowerSafeFormType(item->GetFormType(), isCookingStation)) {
                        continue;
                    }
                    outCache[item] += data.first;
                }
            }
        }

        return favoritedItems.size();
    }

}  // namespace Services
