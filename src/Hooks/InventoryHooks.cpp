#include "Hooks/InventoryHooks.h"
#include "Hooks/CraftingSession.h"
#include "Services/ContainerManager.h"
#include "Services/ContainerRegistry.h"
#include "Services/INISettings.h"
#include "Services/SourceScanner.h"

#include <MinHook.h>
#include <Windows.h>
#include <chrono>
#include <atomic>
#include <unordered_set>

// Performance timing helper
#define SCIE_TIMED_SCOPE(name) \
    auto _timer_start_##__LINE__ = std::chrono::high_resolution_clock::now(); \
    auto _timer_log_##__LINE__ = [&, _start = _timer_start_##__LINE__]() { \
        auto _end = std::chrono::high_resolution_clock::now(); \
        auto _ms = std::chrono::duration<double, std::milli>(_end - _start).count(); \
        logger::debug("[PERF] {} took {:.2f}ms", name, _ms); \
    }; \
    struct _TimerGuard_##__LINE__ { \
        decltype(_timer_log_##__LINE__)& log; \
        ~_TimerGuard_##__LINE__() { log(); } \
    } _guard_##__LINE__{_timer_log_##__LINE__}

namespace Hooks::InventoryHooks {
    namespace {
        // Original function pointers (populated by MinHook)
        GetContainerItemCount_t _originalGetContainerItemCount = nullptr;
        GetInventoryItemEntryAtIdx_t _originalGetInventoryItemEntryAtIdx = nullptr;
        GetInventoryItemCount_t _originalGetInventoryItemCount = nullptr;
        RemoveItem_t _originalRemoveItem = nullptr;

        // VR-only: Hook 3b original pointer (standalone GetItemCount at VR 0x1f7f10)
        // SE/AE never install this hook — the pointer stays null.
        GetInventoryItemCount_t _originalGetInventoryItemCount_VR = nullptr;

        // Track if hooks are installed
        bool s_hooksInstalled = false;

        // Performance tracking for GetInventoryItemCount
        std::atomic<int> s_getItemCountCalls{0};

        /// Detect station type from furniture's workbench data
        /// Returns StationType based on BenchType enum in CommonLibSSE-NG
        /// Returns Unknown for unrecognized stations - no filtering will be applied
        Services::StationType GetStationTypeFromFurniture(RE::TESObjectREFR* a_furniture) {
            using Services::StationType;

            if (!a_furniture) {
                logger::debug("GetStationTypeFromFurniture: null furniture - no filtering");
                return StationType::Unknown;
            }

            auto* base = a_furniture->GetBaseObject();
            if (!base) {
                logger::debug("GetStationTypeFromFurniture: no base object - no filtering");
                return StationType::Unknown;
            }

            auto* furn = base->As<RE::TESFurniture>();
            if (!furn) {
                logger::debug("GetStationTypeFromFurniture: not furniture type - no filtering");
                return StationType::Unknown;
            }

            // WorkBenchData contains benchType field
            auto benchType = furn->workBenchData.benchType.get();

            // Log the bench type for debugging station-specific issues
            const char* furnName = furn->GetName() ? furn->GetName() : "unnamed";
            logger::info("Detected furniture '{}' with BenchType {} (raw value: {})",
                furnName, static_cast<int>(benchType), static_cast<int>(benchType));

            // Map BenchType to StationType per feature plan
            // See docs/Architecture.md "Appendix: Crafting Station Reference"
            switch (benchType) {
                case RE::TESFurniture::WorkBenchData::BenchType::kCreateObject:
                    // Forge, smelter, tanning rack, cooking pot, staff enchanter, carpenter
                    return StationType::Crafting;

                case RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon:
                case RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor:
                    // Grindstone, armor workbench
                    return StationType::Tempering;

                case RE::TESFurniture::WorkBenchData::BenchType::kEnchanting:
                    return StationType::Enchanting;

                case RE::TESFurniture::WorkBenchData::BenchType::kAlchemy:
                    return StationType::Alchemy;

                default:
                    // Unknown bench type - no filtering (allow all form types)
                    logger::info("Unknown bench type {} - no filtering will be applied",
                        static_cast<int>(benchType));
                    return StationType::Unknown;
            }
        }


        /// Refresh source item counts and index mapping after crafting
        /// Called when needsCacheRefresh is set — re-queries each source's actual
        /// item count from the engine and recalculates startIndex values.
        /// Also rebuilds the inventory cache (merged item counts).
        /// This is necessary because enchanting (and other crafting) mutates the
        /// player's InventoryChanges directly (adding crafted items, removing base
        /// items), which shifts entry indices without our hooks knowing.
        void RefreshSessionAfterCraft() {
            if (!g_craftingSession.active) return;

            // Phase 1: Refresh source counts and index mapping
            std::int32_t currentIndex = 0;
            for (auto& source : g_craftingSession.sources) {
                auto* container = source.ref.get().get();
                if (!container) {
                    source.startIndex = currentIndex;
                    source.itemCount = 0;
                    continue;
                }

                std::int32_t newCount = _originalGetContainerItemCount(container, false, true);
                if (newCount != source.itemCount) {
                    logger::debug("RefreshSessionAfterCraft: {} ({:08X}) count {} -> {}",
                        container->IsPlayerRef() ? "Player" :
                            (container->GetBaseObject() ? container->GetBaseObject()->GetName() : "Unknown"),
                        static_cast<std::uint32_t>(container->GetFormID()),
                        source.itemCount, newCount);
                }
                source.itemCount = newCount;
                source.startIndex = currentIndex;
                currentIndex += newCount;
            }

            // Phase 2: Rebuild inventory cache (merged item counts across all sources)
            g_craftingSession.RebuildInventoryCache();

            logger::debug("RefreshSessionAfterCraft: {} sources, {} total items, {} unique cached",
                g_craftingSession.sources.size(), currentIndex, g_craftingSession.inventoryCache.size());
        }

        /// Check if player is using a crafting station and lazily initialize session
        /// Returns true if session is active (player at crafting station)
        bool EnsureSessionActive() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return false;

            // Block during save/load — game state is transitional and container
            // references may be stale or half-initialized. Without this guard,
            // hooks can intercept inventory queries during load with corrupt data.
            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
                if (g_craftingSession.active) {
                    logger::info("Game loading detected, ending crafting session");
                    g_craftingSession.Reset();
                }
                return false;
            }

            // Check if player is using furniture (crafting station)
            auto furnitureHandle = player->GetOccupiedFurniture();
            if (furnitureHandle.native_handle() == 0) {
                // Not at crafting station - clear session if it was active
                if (g_craftingSession.active) {
                    // Log final perf stats
                    if (s_getItemCountCalls > 0) {
                        logger::debug("[PERF] Session ended - GetInventoryItemCount: {} calls (cached)",
                            s_getItemCountCalls.load());
                    }
                    logger::info("Player left crafting station, ending session");
                    g_craftingSession.Reset();
                }
                return false;
            }

            // Player is at furniture - check if session already initialized
            if (g_craftingSession.active) {
                return true;  // Already set up
            }

            // Check if mod is enabled
            if (!Services::INISettings::GetSingleton()->GetEnabled()) {
                return false;
            }

            // Initialize session NOW (lazy init on first query)
            logger::info("Player at crafting station - initializing session");
            auto sessionStartTime = std::chrono::high_resolution_clock::now();

            // Reset perf counters
            s_getItemCountCalls = 0;

            g_craftingSession.Reset();
            g_craftingSession.active = true;

            // Detect station type from furniture for filtering
            auto* furniture = furnitureHandle.get().get();
            g_craftingSession.stationType = GetStationTypeFromFurniture(furniture);
            g_craftingSession.furniture = furnitureHandle;

            // Check if this is a cooking station (for follower ALCH exception)
            auto* containerMgr = Services::ContainerManager::GetSingleton();
            g_craftingSession.isCookingStation = containerMgr->IsCookingStation(furniture);

            logger::info("Station type: {} (from furniture {:08X}){}",
                Services::INISettings::GetStationKeyPrefix(g_craftingSession.stationType),
                furniture ? static_cast<std::uint32_t>(furniture->GetFormID()) : 0,
                g_craftingSession.isCookingStation ? " [cooking]" : "");

            // Use SourceScanner to collect sources and build cache
            Services::ScanConfig scanConfig;
            scanConfig.stationType = g_craftingSession.stationType;
            scanConfig.isCookingStation = g_craftingSession.isCookingStation;
            scanConfig.includeGlobals = true;
            scanConfig.includeFollowers = true;
            scanConfig.maxDistance = 0.0f;  // Use INI setting

            auto scanResult = Services::SourceScanner::GetSingleton()->ScanSources(
                scanConfig, _originalGetContainerItemCount);

            // Transfer scan results to session
            g_craftingSession.sources = std::move(scanResult.sources);
            g_craftingSession.inventoryCache = std::move(scanResult.inventoryCache);
            g_craftingSession.favoritedItemsExcluded = scanResult.favoritedItemsExcluded;

            auto sessionEndTime = std::chrono::high_resolution_clock::now();
            logger::debug("[PERF] === SESSION INIT TOTAL: {:.2f}ms ===",
                std::chrono::duration<double, std::milli>(sessionEndTime - sessionStartTime).count());

            return true;
        }

        // ================================================================
        // Hook 1: GetContainerItemCount
        // Address Library IDs: 19274 (SE) / 19700 (AE)
        // Called when menu queries total item count for a container
        // ================================================================
        std::int32_t Hook_GetContainerItemCount(RE::TESObjectREFR* a_ref, bool a_useMerchant, bool a_unk) {
            // Only intercept for player at crafting station
            if (!a_ref || !a_ref->IsPlayerRef() || !EnsureSessionActive()) {
                return _originalGetContainerItemCount(a_ref, a_useMerchant, a_unk);
            }

            // Refresh source counts if crafting mutated inventories
            if (g_craftingSession.needsCacheRefresh) {
                RefreshSessionAfterCraft();
            }

            // Return combined count from all sources
            std::int32_t total = g_craftingSession.GetTotalItemCount();
            logger::debug("GetContainerItemCount(player): {} items from {} sources",
                total, g_craftingSession.sources.size());
            return total;
        }

        // ================================================================
        // Hook 2: GetInventoryItemEntryAtIdx
        // Address Library IDs: 19273 (SE) / 19699 (AE)
        // Called when menu requests item at specific index
        //
        // Two-layer filtering based on CPRFC's approach:
        //
        // 1. FILTER: Weapons and armor ONLY from player inventory
        //    Prevents duplicate entries at enchanting/tempering stations
        //    and ensures the enchanted/tempered result goes to player inv.
        //
        // 2. DEDUPLICATE: Everything else (soul gems, ingredients, etc.)
        //    Some menus iterate items instead of checking counts (alchemy,
        //    soul gem picker). We merge counts from multiple containers
        //    into the first entry seen for each item type.
        //
        // https://github.com/JerryYOJ/CraftingPullFromContainer-SKSE
        // ================================================================
        RE::InventoryEntryData* Hook_GetInventoryItemEntryAtIdx(RE::TESObjectREFR* a_ref,
            std::int32_t a_idx, bool a_useMerchant)
        {
            // Only intercept for player at crafting station
            if (!a_ref || !a_ref->IsPlayerRef() || !EnsureSessionActive()) {
                return _originalGetInventoryItemEntryAtIdx(a_ref, a_idx, a_useMerchant);
            }

            // Refresh source counts if crafting mutated inventories
            // Must happen BEFORE index lookup — stale startIndex/itemCount causes
            // out-of-bounds access (nexusphere crash signature at SkyrimSE+02E1FD0).
            // Hook 1 and Hook 3 also check this, but DescriptionFramework (or the
            // game) can call Hook 2 directly without Hook 1 running first.
            if (g_craftingSession.needsCacheRefresh) {
                logger::debug("GetInventoryItemEntryAtIdx: refreshing cache before index lookup");
                RefreshSessionAfterCraft();
            }

            // Clear deduplication cache when menu starts a new iteration (index 0)
            // This prevents count inflation if the menu re-queries all items
            if (a_idx == 0 && !g_craftingSession.seenItems.empty()) {
                logger::trace("GetInventoryItemEntryAtIdx: clearing dedup cache (new iteration)");
                g_craftingSession.seenItems.clear();
            }

            // Find which source owns this index
            auto* source = g_craftingSession.FindSourceForIndex(a_idx);
            if (!source) {
                logger::warn("GetInventoryItemEntryAtIdx: index {} out of range", a_idx);
                return nullptr;
            }

            // Get the container for this source
            auto containerHandle = source->ref;
            auto* container = containerHandle.get().get();
            if (!container) {
                logger::warn("GetInventoryItemEntryAtIdx: source container no longer valid");
                return nullptr;
            }

            // Calculate local index within this container
            std::int32_t localIdx = g_craftingSession.GetLocalIndex(source, a_idx);

            // Defensive bounds check - catch stale index mapping before calling engine
            if (localIdx < 0 || localIdx >= source->itemCount) {
                logger::error("Hook2 bounds fail: idx={}, localIdx={}, source.itemCount={}, startIndex={}, needsRefresh={}, container={:08X}",
                    a_idx, localIdx, source->itemCount, source->startIndex,
                    g_craftingSession.needsCacheRefresh,
                    static_cast<std::uint32_t>(container->GetFormID()));
                return nullptr;
            }

            // Get the entry from the actual container
            auto* entry = _originalGetInventoryItemEntryAtIdx(container, localIdx, false);

            // Defensive pointer validation - catch garbage/sentinel returns from engine
            if (!entry) {
                return nullptr;
            }
            if (reinterpret_cast<std::uintptr_t>(entry) < 0x10000) {
                logger::error("Hook2 bad entry: {:p} for container {:08X} localIdx={}, source.itemCount={}",
                    static_cast<void*>(entry),
                    static_cast<std::uint32_t>(container->GetFormID()),
                    localIdx, source->itemCount);
                return nullptr;
            }

            // Note: Use entry->object directly to avoid Windows GetObject macro collision
            if (!entry->object) {
                return nullptr;
            }

            auto* item = entry->object;

            // ----------------------------------------------------------------
            // Layer 1 - FILTER: Check if this form type should be pulled from containers
            // Configured per-station via MCM Filtering tab.
            // Player inventory items always pass through (it's their inventory).
            // ----------------------------------------------------------------
            if (!container->IsPlayerRef()) {
                auto* settings = Services::INISettings::GetSingleton();
                if (!settings->ShouldPullFormType(g_craftingSession.stationType, item->GetFormType())) {
                    logger::trace("GetInventoryItemEntryAtIdx: filtering {} from container (form type {} disabled for {})",
                        item->GetName(),
                        static_cast<int>(item->GetFormType()),
                        Services::INISettings::GetStationKeyPrefix(g_craftingSession.stationType));
                    return nullptr;
                }

                // Apply follower safety filter
                if (source->isFollower && !IsFollowerSafeFormType(item->GetFormType(), g_craftingSession.isCookingStation)) {
                    logger::trace("GetInventoryItemEntryAtIdx: filtering {} from follower (unsafe form type {})",
                        item->GetName(), static_cast<int>(item->GetFormType()));
                    return nullptr;
                }
            }

            // Weapons and armor skip deduplication - each instance can be unique
            // (enchantments, tempering levels, etc.)
            if (item->IsWeapon() || item->IsArmor()) {
                return entry;
            }

            // ----------------------------------------------------------------
            // Layer 2 - DEDUPLICATE: Materials (soul gems, ingredients, etc.)
            // Some menus iterate items instead of checking counts. Merge
            // counts from multiple containers into the first entry seen.
            //
            // Strategy: return the ORIGINAL entry with countDelta set to the
            // merged total from inventoryCache. This mutates the source
            // container's InventoryEntryData, but is the only approach that
            // works — copies (with or without extraLists) cause CTD at
            // enchanting tables. The mutation is harmless in practice: the
            // game recalculates inventory on next access, and v2.5.0 shipped
            // this approach without issues. Suppress duplicates with nullptr.
            // ----------------------------------------------------------------
            auto it = g_craftingSession.seenItems.find(item);
            if (it != g_craftingSession.seenItems.end()) {
                // Duplicate — suppress it. The first-seen entry has merged counts.
                logger::trace("GetInventoryItemEntryAtIdx: deduplicating {} - suppressing duplicate (count {} from {})",
                    item->GetName(), entry->countDelta, container->IsPlayerRef() ? "player" : "container");
                return nullptr;
            }

            // First time seeing this item — set countDelta to merged total
            // from the inventory cache, then return the original entry.
            // extraLists is preserved (critical for soul gem fill state,
            // enchantment data, etc. — null or copied extraLists both CTD).
            auto cachedIt = g_craftingSession.inventoryCache.find(item);
            if (cachedIt != g_craftingSession.inventoryCache.end()) {
                entry->countDelta = cachedIt->second;
            }
            g_craftingSession.seenItems[item] = entry;
            logger::trace("GetInventoryItemEntryAtIdx: first {} x{} (merged from cache, original entry from {})",
                item->GetName(), entry->countDelta,
                container->IsPlayerRef() ? "player" : "container");
            return entry;
        }

        // ================================================================
        // Hook 3: GetInventoryItemCount
        // Address Library IDs: 15869 (SE) / 16109 (AE)
        // Called when menu needs count of a specific item
        // ================================================================
        std::int32_t Hook_GetInventoryItemCount(RE::InventoryChanges* a_inv,
            RE::TESBoundObject* a_item, void* a_filter)
        {
            // Only intercept at crafting station
            if (!EnsureSessionActive()) {
                return _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            }

            // Check if this is player's inventory changes
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            }

            auto* playerInv = player->GetInventoryChanges();
            if (a_inv != playerInv) {
                // VR path: VR's COBJ system queries a different InventoryChanges for recipe
                // availability checks. Check our cache anyway - if the item is there, return
                // our merged count. This fixes VR recipe availability for container items.
                auto cachedCount = g_craftingSession.GetCachedItemCount(a_item);
                if (cachedCount > 0) {
                    logger::trace("GetInventoryItemCount({}): returning {} (VR bypass - cached)",
                        a_item->GetName(), cachedCount);
                    return cachedCount;
                }
                // Item not in cache - fall through to original
                return _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            }

            // Refresh cache and source indices if flagged (after crafting)
            if (g_craftingSession.needsCacheRefresh) {
                logger::trace("GetInventoryItemCount: refreshing cache after craft");
                RefreshSessionAfterCraft();
            }

            // Use cached inventory for fast lookup (cache built at session init)
            std::int32_t total = g_craftingSession.GetCachedItemCount(a_item);

            // Track call stats (lightweight now, but keep for verification)
            s_getItemCountCalls++;

            // Log every 500 calls (less frequent since it's fast now)
            if (s_getItemCountCalls % 500 == 0) {
                logger::trace("[PERF] GetInventoryItemCount: {} calls (using cache)",
                    s_getItemCountCalls.load());
            }

            logger::trace("GetInventoryItemCount({}): returning {} (cached)",
                a_item ? a_item->GetName() : "null", total);
            return total;
        }

        // ================================================================
        // Hook 3b: VR-only GetInventoryItemCount (standalone variant)
        // VR offset: 0x1f7f10 (VR equivalent of SE 15869)
        //
        // On SE/AE, Hook 3 intercepts SE 15869 (the standalone function).
        // On VR, Hook 3's VariantID resolves to 0x1f7ed0, which is the VR
        // equivalent of SE 15868 (the member function) — not SE 15869.
        // The standalone function at VR 0x1f7f10 is used by
        // ConstructibleObjectMenu::SetItemCardInfo to populate material
        // count display and by the craft execution path. Without this
        // hook, VR shows "0" for material counts and crafting fails.
        //
        // This hook is NEVER installed on SE/AE.
        // ================================================================
        std::int32_t Hook_GetInventoryItemCount_VR(RE::InventoryChanges* a_inv,
            RE::TESBoundObject* a_item, void* a_filter)
        {
            if (!EnsureSessionActive()) {
                return _originalGetInventoryItemCount_VR(a_inv, a_item, a_filter);
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return _originalGetInventoryItemCount_VR(a_inv, a_item, a_filter);
            }

            // Refresh cache if flagged (after crafting)
            if (g_craftingSession.needsCacheRefresh) {
                logger::trace("GetInventoryItemCount_VR: refreshing cache after craft");
                RefreshSessionAfterCraft();
            }

            // Check cache — return merged count if item is tracked
            auto cachedCount = g_craftingSession.GetCachedItemCount(a_item);
            if (cachedCount > 0) {
                logger::trace("GetInventoryItemCount_VR({}): returning {} (cached)",
                    a_item ? a_item->GetName() : "null", cachedCount);
                return cachedCount;
            }

            // Item not in cache — fall through to original
            return _originalGetInventoryItemCount_VR(a_inv, a_item, a_filter);
        }

        // ================================================================
        // Hook 4: RemoveItem (via VTable)
        // VTable index: 0x56
        // Called when crafting consumes materials
        //
        // NOTE: x64 MSVC struct return ABI - result pointer is hidden second param!
        // Signature: RemoveItem(this, result*, item, count, reason, extra, moveTo, dropLoc, rotate)
        //
        // Based on CraftingPullFromContainers implementation.
        // ================================================================
        RE::ObjectRefHandle* Hook_RemoveItem(RE::TESObjectREFR* a_this, RE::ObjectRefHandle* a_result,
            RE::TESBoundObject* a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extraList, RE::TESObjectREFR* a_moveToRef,
            const RE::NiPoint3* a_dropLoc, const RE::NiPoint3* a_rotate)
        {
            // Only intercept during active crafting session with kRemove reason
            // Following CraftingPullFromContainers approach
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !g_craftingSession.active || !a_item ||
                a_reason != RE::ITEM_REMOVE_REASON::kRemove)
            {
                return _originalRemoveItem(a_this, a_result, a_item, a_count, a_reason,
                    a_extraList, a_moveToRef, a_dropLoc, a_rotate);
            }

            std::int32_t remaining = a_count;

            // Iterate through cached sources, remove from containers first
            for (auto& source : g_craftingSession.sources) {
                if (remaining <= 0) break;

                auto* container = source.ref.get().get();
                if (!container) continue;

                // Check how many of this item the container has
                auto countsMap = container->GetInventoryCounts();
                auto it = countsMap.find(a_item);
                if (it == countsMap.end() || it->second <= 0) {
                    continue;
                }

                std::int32_t available = it->second;
                std::int32_t toRemove = std::min(remaining, available);

                if (toRemove > 0) {
                    logger::trace("RemoveItem: {} x{} from {:08X}",
                        a_item->GetName(), toRemove,
                        static_cast<std::uint32_t>(container->GetFormID()));

                    // For actors (player/NPCs), use _originalRemoveItem
                    // For regular containers, use container->RemoveItem directly
                    if (container->Is(RE::FormType::ActorCharacter)) {
                        _originalRemoveItem(container, a_result, a_item, toRemove, a_reason,
                            a_extraList, a_moveToRef, a_dropLoc, a_rotate);
                    } else {
                        container->RemoveItem(a_item, toRemove, a_reason, a_extraList,
                            a_moveToRef, a_dropLoc, a_rotate);
                    }

                    remaining -= toRemove;
                }
            }

            if (remaining > 0) {
                logger::warn("RemoveItem: {} x{} could not be fulfilled from sources!",
                    a_item->GetName(), remaining);
            }

            // Update inventory cache to reflect consumed items
            std::int32_t actuallyRemoved = a_count - remaining;
            if (actuallyRemoved > 0) {
                g_craftingSession.DecrementCachedCount(a_item, actuallyRemoved);
                logger::trace("RemoveItem: cache updated, {} x{} removed",
                    a_item->GetName(), actuallyRemoved);

                // Flag cache for refresh on next query to pick up newly crafted items
                g_craftingSession.needsCacheRefresh = true;
            }

            // Only call original if we couldn't fulfill the full request.
            // When remaining=0, ALL items were already removed from sources —
            // calling original with count=0 and a_extraList risks passing a stale
            // ExtraDataList pointer (the per-source removal may have freed/modified it).
            if (remaining > 0) {
                logger::trace("RemoveItem: passing remaining {} to original", remaining);
                return _originalRemoveItem(a_this, a_result, a_item, remaining, a_reason,
                    a_extraList, a_moveToRef, a_dropLoc, a_rotate);
            }

            // Everything handled — return result from last successful removal
            return a_result;
        }
    }  // anonymous namespace

    /// Show a user-friendly error message box
    void ShowDependencyError(const char* details) {
        std::string msg = fmt::format(
            "SCIE could not initialize due to a dependency problem.\n\n"
            "Error: {}\n\n"
            "Please ensure you have:\n"
            "  1. The correct Skyrim version (check mod page)\n"
            "  2. Address Library for SKSE Plugins installed\n"
            "     (Use 'All in One' version from Nexus)\n"
            "  3. SKSE64 installed correctly\n\n"
            "SCIE will be disabled for this session.\n"
            "The game will continue to work normally without SCIE.",
            details
        );

        // Log the error
        logger::critical("DEPENDENCY ERROR: {}", details);

        // Show message box (will appear before main menu)
        MessageBoxA(nullptr, msg.c_str(), "SCIE - Setup Required", MB_OK | MB_ICONWARNING);
    }

    bool Install() {
        logger::info("Installing inventory hooks (zero-transfer architecture)...");

        // ================================================================
        // Step 1: Verify Address Library is working BEFORE we try to use it
        // This catches missing/wrong AddressLib early with a helpful message
        // ================================================================
        try {
            // Test lookup - if AddressLib is missing or wrong version, this throws
            REL::Relocation<std::uintptr_t> testLookup{ REL::VariantID(19274, 19700, 0x29f980) };
            if (testLookup.address() == 0) {
                ShowDependencyError("Address Library returned invalid address (0x0).\n"
                    "Your Address Library version may not match your Skyrim version.");
                return false;
            }
            logger::info("Address Library check passed (test address: {:X})", testLookup.address());
        } catch (const std::exception& e) {
            ShowDependencyError(fmt::format("Address Library lookup failed: {}", e.what()).c_str());
            return false;
        } catch (...) {
            ShowDependencyError("Address Library lookup failed with unknown error.\n"
                "Address Library may be missing or corrupted.");
            return false;
        }

        // ================================================================
        // Step 2: Initialize MinHook
        // ================================================================
        if (MH_Initialize() != MH_OK) {
            logger::error("Failed to initialize MinHook");
            ShowDependencyError("MinHook initialization failed.\n"
                "This is unusual - try reinstalling the mod.");
            return false;
        }

        bool success = true;

        // ================================================================
        // Step 3: Install hooks (Address Library verified, should work now)
        // ================================================================
        try {
            // Hook 1: GetContainerItemCount (19274/19700)
            {
                REL::Relocation<std::uintptr_t> target{ REL::VariantID(19274, 19700, 0x29f980) };
                if (MH_CreateHook(reinterpret_cast<void*>(target.address()),
                                 reinterpret_cast<void*>(&Hook_GetContainerItemCount),
                                 reinterpret_cast<void**>(&_originalGetContainerItemCount)) != MH_OK)
                {
                    logger::error("Failed to create GetContainerItemCount hook");
                    success = false;
                } else {
                    logger::info("GetContainerItemCount hook created at {:X}", target.address());
                }
            }

            // Hook 2: GetInventoryItemEntryAtIdx (19273/19699)
            {
                REL::Relocation<std::uintptr_t> target{ REL::VariantID(19273, 19699, 0x29f910) };
                if (MH_CreateHook(reinterpret_cast<void*>(target.address()),
                                 reinterpret_cast<void*>(&Hook_GetInventoryItemEntryAtIdx),
                                 reinterpret_cast<void**>(&_originalGetInventoryItemEntryAtIdx)) != MH_OK)
                {
                    logger::error("Failed to create GetInventoryItemEntryAtIdx hook");
                    success = false;
                } else {
                    logger::info("GetInventoryItemEntryAtIdx hook created at {:X}", target.address());
                }
            }

            // Hook 3: GetInventoryItemCount (15869/16109)
            // On SE/AE: hooks the standalone function at SE 15869 / AE 16109
            // On VR: hooks the member function at VR 0x1f7ed0 (SE 15868 equivalent)
            {
                REL::Relocation<std::uintptr_t> target{ REL::VariantID(15869, 16109, 0x1f7ed0) };
                if (MH_CreateHook(reinterpret_cast<void*>(target.address()),
                                 reinterpret_cast<void*>(&Hook_GetInventoryItemCount),
                                 reinterpret_cast<void**>(&_originalGetInventoryItemCount)) != MH_OK)
                {
                    logger::error("Failed to create GetInventoryItemCount hook");
                    success = false;
                } else {
                    logger::info("GetInventoryItemCount hook created at {:X}", target.address());
                }
            }

            // Hook 3b: VR-only standalone GetInventoryItemCount (VR 0x1f7f10)
            // On VR, Hook 3 catches the member function (VR 0x1f7ed0) used by
            // the COBJ condition chain. But SetItemCardInfo (material count display)
            // and the craft execution path use the standalone function at VR 0x1f7f10.
            // This hook is NEVER installed on SE/AE.
            if (REL::Module::IsVR()) {
                auto vrAddr = REL::Module::get().base() + 0x1f7f10;
                if (MH_CreateHook(reinterpret_cast<void*>(vrAddr),
                                 reinterpret_cast<void*>(&Hook_GetInventoryItemCount_VR),
                                 reinterpret_cast<void**>(&_originalGetInventoryItemCount_VR)) != MH_OK)
                {
                    logger::error("Failed to create VR GetInventoryItemCount hook at {:X}", vrAddr);
                    success = false;
                } else {
                    logger::info("VR GetInventoryItemCount hook created at {:X} (standalone variant)", vrAddr);
                }
            }

            // Hook 4: RemoveItem via VTable[0x56]
            {
                _originalRemoveItem = reinterpret_cast<RemoveItem_t>(
                    REL::Relocation<std::uintptr_t>(RE::VTABLE_PlayerCharacter[0])
                        .write_vfunc(0x56, Hook_RemoveItem));

                logger::info("RemoveItem VTable hook installed at VTable[0x56]");
            }
        } catch (const std::exception& e) {
            logger::error("Exception during hook installation: {}", e.what());
            ShowDependencyError(fmt::format("Hook installation failed: {}", e.what()).c_str());
            MH_Uninitialize();
            return false;
        } catch (...) {
            logger::error("Unknown exception during hook installation");
            ShowDependencyError("Hook installation failed with unknown error.");
            MH_Uninitialize();
            return false;
        }

        // Enable all MinHook hooks
        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            logger::error("Failed to enable MinHook hooks");
            success = false;
        }

        s_hooksInstalled = success;

        if (success) {
            logger::info("All inventory hooks installed successfully");
        } else {
            logger::error("Some inventory hooks failed to install");
        }

        return success;
    }

    void Uninstall() {
        if (!s_hooksInstalled) return;

        logger::info("Uninstalling inventory hooks...");

        // Disable and remove MinHook hooks
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        // Note: VTable hook is not restored (would need to save/restore original)
        // This is typically fine since plugin unload only happens on game exit

        s_hooksInstalled = false;
        logger::info("Inventory hooks uninstalled");
    }

    GetContainerItemCount_t GetOriginalGetContainerItemCount() {
        return _originalGetContainerItemCount;
    }

    GetInventoryItemEntryAtIdx_t GetOriginalGetInventoryItemEntryAtIdx() {
        return _originalGetInventoryItemEntryAtIdx;
    }

    GetInventoryItemCount_t GetOriginalGetInventoryItemCount() {
        return _originalGetInventoryItemCount;
    }
}
