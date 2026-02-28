#pragma once

#include <unordered_map>
#include <unordered_set>
#include "Services/INISettings.h"

namespace Hooks {
    /// Represents a container contributing to the virtual inventory
    struct ContainerSource {
        RE::ObjectRefHandle ref;       // Container reference
        std::int32_t itemCount;        // Cached count (for index mapping)
        std::int32_t startIndex;       // Starting index in virtual inventory
        bool isFollower = false;       // True for follower/spouse sources (safety filter applied)
    };

    /// Check if a form type is safe to take from followers
    /// Blocks weapons, armor, ammo (always), and alchemy items (except at cooking stations)
    inline bool IsFollowerSafeFormType(RE::FormType a_type, bool a_isCooking) {
        switch (a_type) {
            case RE::FormType::Weapon:
            case RE::FormType::Armor:
            case RE::FormType::Ammo:
                return false;
            case RE::FormType::AlchemyItem:
                return a_isCooking;
            default:
                return true;
        }
    }

    /// Session state for zero-transfer crafting (reset on menu open)
    struct CraftingSession {
        RE::ObjectRefHandle furniture;   // Current crafting station
        std::vector<ContainerSource> sources;  // Player + containers
        bool active = false;

        /// Flag to trigger cache rebuild on next inventory query
        /// Set after RemoveItem to pick up newly crafted items
        bool needsCacheRefresh = false;

        /// Current station type for filtering (detected from furniture BenchType)
        Services::StationType stationType = Services::StationType::Crafting;

        /// True if the current crafting station is a cooking pot (ALCH allowed from followers)
        bool isCookingStation = false;

        /// Count of item types excluded due to being favorited (for MCM display)
        std::size_t favoritedItemsExcluded = 0;

        /// Cached inventory counts: item -> total count across all sources
        /// Populated once at session start, updated on RemoveItem
        std::unordered_map<RE::TESBoundObject*, std::int32_t> inventoryCache;

        /// Deduplication cache for GetInventoryItemEntryAtIdx
        /// Maps item type to the first InventoryEntryData seen (original entry,
        /// not a copy) with countDelta set to the merged total from inventoryCache.
        /// Duplicates are suppressed with nullptr.
        std::unordered_map<RE::TESBoundObject*, RE::InventoryEntryData*> seenItems;

        /// Track FormIDs that failed global lookup (VR limitation) to show notification only once
        std::unordered_set<RE::FormID> notifiedFailedGlobals;

        void Reset() {
            furniture = RE::ObjectRefHandle();
            sources.clear();
            inventoryCache.clear();
            seenItems.clear();
            notifiedFailedGlobals.clear();
            active = false;
            needsCacheRefresh = false;
            stationType = Services::StationType::Crafting;
            isCookingStation = false;
            favoritedItemsExcluded = 0;
        }

        /// Rebuild inventory cache from current sources
        /// Called after crafting to pick up newly created items
        void RebuildInventoryCache() {
            inventoryCache.clear();
            seenItems.clear();  // Invalidate stale InventoryEntryData pointers

            // Build set of favorited items (Essential Favorites compatibility)
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

            for (auto& source : sources) {
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
                        if (source.isFollower && !IsFollowerSafeFormType(item->GetFormType(), isCookingStation)) {
                            continue;
                        }
                        inventoryCache[item] += data.first;
                    }
                }
            }
            favoritedItemsExcluded = favoritedItems.size();
            needsCacheRefresh = false;
        }

        /// Get cached count for a specific item (returns 0 if not found)
        std::int32_t GetCachedItemCount(RE::TESBoundObject* a_item) const {
            auto it = inventoryCache.find(a_item);
            return it != inventoryCache.end() ? it->second : 0;
        }

        /// Update cache after removing items (called from RemoveItem hook)
        void DecrementCachedCount(RE::TESBoundObject* a_item, std::int32_t a_count) {
            auto it = inventoryCache.find(a_item);
            if (it != inventoryCache.end()) {
                it->second = std::max(0, it->second - a_count);
            }
        }

        /// Get total item count across all sources
        std::int32_t GetTotalItemCount() const {
            std::int32_t total = 0;
            for (const auto& source : sources) {
                total += source.itemCount;
            }
            return total;
        }

        /// Find which source owns a given virtual index
        /// Returns nullptr if index is out of range
        ContainerSource* FindSourceForIndex(std::int32_t a_index) {
            for (auto& source : sources) {
                if (a_index < source.startIndex + source.itemCount) {
                    return &source;
                }
            }
            return nullptr;
        }

        /// Get the local index within a source for a given virtual index
        std::int32_t GetLocalIndex(const ContainerSource* a_source, std::int32_t a_virtualIndex) const {
            if (!a_source) return -1;
            return a_virtualIndex - a_source->startIndex;
        }
    };

    /// Global session instance
    inline CraftingSession g_craftingSession;

    /// Session management functions
    class CraftingSessionManager {
    public:
        static CraftingSessionManager* GetSingleton();

        /// Called when a crafting menu opens
        void OnCraftingMenuOpen(RE::TESObjectREFR* a_furniture);

        /// Called when the crafting menu closes
        void OnCraftingMenuClose();

        /// Check if a crafting session is active
        bool IsSessionActive() const { return g_craftingSession.active; }

        /// Get current session (for hooks to access)
        CraftingSession& GetSession() { return g_craftingSession; }

    private:
        CraftingSessionManager() = default;
        ~CraftingSessionManager() = default;
        CraftingSessionManager(const CraftingSessionManager&) = delete;
        CraftingSessionManager& operator=(const CraftingSessionManager&) = delete;
    };
}
