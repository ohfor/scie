#include "Papyrus/PapyrusInterface.h"
#include "Services/ContainerManager.h"
#include "Services/ContainerRegistry.h"
#include "Services/ContainerUtils.h"
#include "Services/INISettings.h"
#include "Services/TranslationService.h"

namespace Papyrus {
    namespace {
        constexpr std::string_view ScriptName = "SCIE_NativeFunctions"sv;

        /// Helper to check if mod is enabled
        inline bool IsModEnabled() {
            return Services::INISettings::GetSingleton()->GetEnabled();
        }

        /// Dump all containers in current cell to SKSE log
        /// Note: Works even when mod is disabled (debug utility)
        void DumpContainers(RE::StaticFunctionTag*) {
            Services::ContainerRegistry::GetSingleton()->DumpContainersToLog();
        }

        /// Papyrus wrapper: check if a container has the respawn flag
        bool IsContainerUnsafe(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref) {
            return Services::IsContainerUnsafe(a_ref);
        }

        /// Get display name for a container, with " - UNSAFE" suffix if it has the respawn flag
        RE::BSFixedString GetContainerDisplayName(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref) {
            if (!a_ref) return "";

            const char* name = a_ref->GetDisplayFullName();
            std::string result = (name && name[0]) ? name : "Container";

            if (Services::IsContainerUnsafe(a_ref)) {
                result += Services::TranslationService::GetSingleton()->GetTranslation("$SCIE_SuffixUnsafe");
            }

            return result;
        }

        /// Check if a reference is a valid target for the toggle power
        /// Valid targets: containers (base type Container) or player-owned mounts (horse saddlebags)
        bool IsValidToggleTarget(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref) {
            if (!a_ref) return false;

            // Check if it's a container
            auto* baseObj = a_ref->GetBaseObject();
            if (baseObj && baseObj->GetFormType() == RE::FormType::Container) {
                return true;
            }

            // Check if it's a player-owned mount (horse saddlebag)
            if (Services::ContainerManager::GetSingleton()->IsPlayerOwnedMount(a_ref)) {
                return true;
            }

            return false;
        }

        /// Toggle a container's crafting source status through 3 states
        /// Returns: 0=off, 1=local, 2=global
        std::int32_t ToggleCraftingContainer(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!IsModEnabled()) return 0;
            if (!a_container) return 0;

            int newState = Services::ContainerRegistry::GetSingleton()->ToggleCycleState(
                a_container->GetFormID(), a_container);
            // Invalidate container cache so changes take effect immediately
            Services::ContainerManager::GetSingleton()->RefreshContainerCache();
            return static_cast<std::int32_t>(newState);
        }

        /// Get the effective state of a container (considers INI + overrides)
        /// Returns: 0=off, 1=local, 2=global
        std::int32_t GetContainerState(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!a_container) return 0;
            return static_cast<std::int32_t>(
                Services::ContainerRegistry::GetSingleton()->GetContainerState(a_container->GetFormID()));
        }

        /// Check if a container is currently active for crafting
        /// Returns true if the container should be used as a crafting source
        bool IsCraftingContainerActive(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!IsModEnabled()) return false;
            if (!a_container) return false;

            return Services::ContainerRegistry::GetSingleton()->ShouldPullFrom(a_container);
        }

        // Legacy functions - still supported for backwards compatibility
        void RegisterCraftingContainer(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!IsModEnabled()) return;
            if (!a_container) return;

            auto* registry = Services::ContainerRegistry::GetSingleton();
            // If not already active, toggle it on (will go to local state)
            if (!registry->ShouldPullFrom(a_container)) {
                registry->ToggleCycleState(a_container->GetFormID(), a_container);
                Services::ContainerManager::GetSingleton()->RefreshContainerCache();
            }
        }

        void UnregisterCraftingContainer(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!IsModEnabled()) return;
            if (!a_container) return;

            auto* registry = Services::ContainerRegistry::GetSingleton();
            // If currently active, set to off
            if (registry->ShouldPullFrom(a_container)) {
                registry->SetContainerState(a_container->GetFormID(), Services::ContainerState::kOff);
                Services::ContainerManager::GetSingleton()->RefreshContainerCache();
            }
        }

        bool IsContainerRegistered(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!IsModEnabled()) return false;
            if (!a_container) return false;

            // Check if container is active (either via INI or player override)
            return Services::ContainerRegistry::GetSingleton()->ShouldPullFrom(a_container);
        }

        /// Check if a container is configured as a global container (accessible from anywhere)
        bool IsGlobalContainer(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container) {
            if (!a_container) return false;

            return Services::ContainerRegistry::GetSingleton()->IsGlobalContainer(a_container->GetFormID());
        }

        /// Get all containers in current cell that would be used for crafting
        /// (player-enabled OR INI-configured, but not player-disabled)
        std::vector<RE::TESObjectREFR*> GetEnabledContainersInCell(RE::StaticFunctionTag*, float a_maxDistance) {
            if (!IsModEnabled()) return {};

            return Services::ContainerRegistry::GetSingleton()->GetEnabledContainersInCell(a_maxDistance);
        }

        /// Get all containers in current cell that were explicitly disabled by player
        std::vector<RE::TESObjectREFR*> GetDisabledContainersInCell(RE::StaticFunctionTag*, float a_maxDistance) {
            if (!IsModEnabled()) return {};

            return Services::ContainerRegistry::GetSingleton()->GetDisabledContainersInCell(a_maxDistance);
        }

        /// Get all containers in current cell that are in global state
        std::vector<RE::TESObjectREFR*> GetGlobalContainersInCell(RE::StaticFunctionTag*, float a_maxDistance) {
            if (!IsModEnabled()) return {};

            return Services::ContainerRegistry::GetSingleton()->GetGlobalContainersInCell(a_maxDistance);
        }

        // ================================================================
        // Player Container Management (for MCM Containers page)
        // ================================================================

        /// Get count of player-overridden containers (cosave entries)
        std::int32_t GetPlayerContainerCount(RE::StaticFunctionTag*) {
            return static_cast<std::int32_t>(
                Services::ContainerRegistry::GetSingleton()->GetPlayerOverrideCount());
        }

        /// Get paginated container names (sorted by location then name)
        std::vector<RE::BSFixedString> GetPlayerContainerNames(RE::StaticFunctionTag*, std::int32_t a_page, std::int32_t a_pageSize) {
            auto page = Services::ContainerRegistry::GetSingleton()->GetPlayerContainerPage(a_page, a_pageSize);
            std::vector<RE::BSFixedString> result;
            result.reserve(page.names.size());
            for (const auto& name : page.names) {
                result.emplace_back(name);
            }
            return result;
        }

        /// Get paginated container locations (parallel array with names)
        std::vector<RE::BSFixedString> GetPlayerContainerLocations(RE::StaticFunctionTag*, std::int32_t a_page, std::int32_t a_pageSize) {
            auto page = Services::ContainerRegistry::GetSingleton()->GetPlayerContainerPage(a_page, a_pageSize);
            std::vector<RE::BSFixedString> result;
            result.reserve(page.locations.size());
            for (const auto& loc : page.locations) {
                result.emplace_back(loc);
            }
            return result;
        }

        /// Get paginated container states (parallel array: 0=off, 1=local, 2=global)
        std::vector<std::int32_t> GetPlayerContainerStates(RE::StaticFunctionTag*, std::int32_t a_page, std::int32_t a_pageSize) {
            auto page = Services::ContainerRegistry::GetSingleton()->GetPlayerContainerPage(a_page, a_pageSize);
            std::vector<std::int32_t> result;
            result.reserve(page.states.size());
            for (int state : page.states) {
                result.push_back(static_cast<std::int32_t>(state));
            }
            return result;
        }

        /// Set a player container's state by sorted index (for MCM click-to-cycle)
        /// a_index: 0-based index into the sorted container list (page * pageSize + offset)
        /// a_newState: 0=off, 1=local, 2=global
        void SetPlayerContainerState(RE::StaticFunctionTag*, std::int32_t a_index, std::int32_t a_newState) {
            if (a_newState < 0 || a_newState > 2) return;

            Services::ContainerRegistry::GetSingleton()->SetContainerStateByIndex(
                a_index, static_cast<Services::ContainerState>(a_newState));
            // Invalidate container cache
            Services::ContainerManager::GetSingleton()->RefreshContainerCache();
        }

        /// Remove all player container overrides for a location (for MCM per-section clear button)
        /// Returns count of removed overrides
        std::int32_t RemovePlayerContainersByLocation(RE::StaticFunctionTag*, RE::BSFixedString a_location) {
            int removed = Services::ContainerRegistry::GetSingleton()->RemovePlayerContainersByLocation(
                a_location.c_str());
            Services::ContainerManager::GetSingleton()->RefreshContainerCache();
            return static_cast<std::int32_t>(removed);
        }

        // ================================================================
        // MCM Configuration Functions
        // ================================================================

        /// Get the current max container distance setting
        float GetMaxDistance(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetMaxContainerDistance();
        }

        /// Set the max container distance and save to INI
        void SetMaxDistance(RE::StaticFunctionTag*, float a_distance) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetMaxContainerDistance(a_distance);
            settings->Save();
        }

        /// Get whether the mod is enabled
        bool GetModEnabled(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetEnabled();
        }

        /// Set whether the mod is enabled and save to INI
        void SetModEnabled(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetEnabled(a_enabled);
            settings->Save();
        }

        /// Get whether debug logging is enabled
        bool GetDebugLogging(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetDebugLogging();
        }

        /// Set whether debug logging is enabled and save to INI
        void SetDebugLogging(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetDebugLogging(a_enabled);
            settings->Save();
        }

        /// Get whether global containers are enabled
        bool GetEnableGlobalContainers(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetEnableGlobalContainers();
        }

        /// Set whether global containers are enabled and save to INI
        void SetEnableGlobalContainers(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetEnableGlobalContainers(a_enabled);
            settings->Save();
        }

        /// Get whether INI-configured containers are enabled
        bool GetEnableINIContainers(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetEnableINIContainers();
        }

        /// Set whether INI-configured containers are enabled and save to INI
        void SetEnableINIContainers(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetEnableINIContainers(a_enabled);
            settings->Save();
        }

        /// Get whether follower inventory is enabled
        bool GetEnableFollowerInventory(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetEnableFollowerInventory();
        }

        /// Set whether follower inventory is enabled and save to INI
        void SetEnableFollowerInventory(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetEnableFollowerInventory(a_enabled);
            settings->Save();
        }

        /// Get whether unsafe (respawning) containers can be toggled
        bool GetAllowUnsafeContainers(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetAllowUnsafeContainers();
        }

        /// Set whether unsafe containers can be toggled and save to INI
        void SetAllowUnsafeContainers(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetAllowUnsafeContainers(a_enabled);
            settings->Save();
        }

        /// Check if Legacy of the Dragonborn is installed
        /// Used by MCM to display compatibility info
        bool IsLOTDInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsLOTDInstalled();
        }

        /// Check if General Stores is installed
        /// Used by MCM to display compatibility info
        bool IsGeneralStoresInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsGeneralStoresInstalled();
        }

        /// Check if Linked Crafting Storage is installed
        /// Used by MCM to display incompatibility warning
        bool IsLinkedCraftingStorageInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsLinkedCraftingStorageInstalled();
        }

        /// Check if Convenient Horses is installed
        bool IsConvenientHorsesInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsConvenientHorsesInstalled();
        }

        /// Check if Hip Bag is installed
        bool IsHipBagInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsHipBagInstalled();
        }

        /// Check if Nether's Follower Framework is installed
        bool IsNFFInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsNFFInstalled();
        }

        /// Check if Essential Favorites SKSE plugin is loaded
        bool IsEssentialFavoritesInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsEssentialFavoritesInstalled();
        }

        /// Check if Favorite Misc Items SKSE plugin is loaded
        bool IsFavoriteMiscItemsInstalled(RE::StaticFunctionTag*) {
            return Services::ContainerRegistry::IsFavoriteMiscItemsInstalled();
        }

        /// Get count of favorited items excluded from current session
        std::int32_t GetFavoritedItemsExcludedCount(RE::StaticFunctionTag*) {
            return static_cast<std::int32_t>(Services::ContainerRegistry::GetFavoritedItemsExcludedCount());
        }

        /// Get count of configured global containers (from INI files)
        std::int32_t GetGlobalContainerCount(RE::StaticFunctionTag*) {
            return static_cast<std::int32_t>(Services::ContainerRegistry::GetSingleton()->GetGlobalContainerCount());
        }

        /// Get count of tracked containers in current cell
        std::int32_t GetTrackedContainerCount(RE::StaticFunctionTag*) {
            if (!IsModEnabled()) return 0;

            auto* settings = Services::INISettings::GetSingleton();
            auto containers = Services::ContainerRegistry::GetSingleton()->GetEnabledContainersInCell(
                settings->GetMaxContainerDistance());
            return static_cast<std::int32_t>(containers.size());
        }

        /// Generate a snapshot INI file from player-marked containers
        /// Returns the filepath, or empty string on failure
        RE::BSFixedString SnapshotToggles(RE::StaticFunctionTag*) {
            auto path = Services::ContainerRegistry::GetSingleton()->GenerateSnapshot();
            return RE::BSFixedString(path);
        }

        /// Clear all player overrides in current cell
        /// Returns count of overrides cleared
        std::int32_t ClearMarkedInCell(RE::StaticFunctionTag*) {
            auto* settings = Services::INISettings::GetSingleton();
            auto result = Services::ContainerRegistry::GetSingleton()->ClearOverridesInCell(
                settings->GetMaxContainerDistance());
            // Invalidate container cache so cleared containers aren't used in next crafting session
            Services::ContainerManager::GetSingleton()->RefreshContainerCache();
            return result;
        }

        // ================================================================
        // Power Management Functions (for mod authors)
        // ================================================================

        /// Get whether powers are auto-granted on game load
        bool GetAutoGrantPowers(RE::StaticFunctionTag*) {
            return Services::INISettings::GetSingleton()->GetAddPowersToPlayer();
        }

        /// Set whether powers are auto-granted on game load and save to INI
        void SetAutoGrantPowers(RE::StaticFunctionTag*, bool a_enabled) {
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetAddPowersToPlayer(a_enabled);
            settings->Save();
        }

        /// Manually grant SCIE powers to player
        /// Returns number of powers added (0-2)
        std::int32_t GrantPowers(RE::StaticFunctionTag*) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                logger::warn("GrantPowers: Player not available");
                return 0;
            }

            auto* dh = RE::TESDataHandler::GetSingleton();
            auto* togglePower = dh ? dh->LookupForm<RE::SpellItem>(0x802, "CraftingInventoryExtender.esp") : nullptr;
            auto* detectPower = dh ? dh->LookupForm<RE::SpellItem>(0x804, "CraftingInventoryExtender.esp") : nullptr;

            std::int32_t added = 0;

            if (togglePower && !player->HasSpell(togglePower)) {
                player->AddSpell(togglePower);
                logger::info("GrantPowers: Added SCIE_TogglePower to player");
                added++;
            }

            if (detectPower && !player->HasSpell(detectPower)) {
                player->AddSpell(detectPower);
                logger::info("GrantPowers: Added SCIE_DetectPower to player");
                added++;
            }

            return added;
        }

        /// Manually revoke SCIE powers from player
        /// Returns number of powers removed (0-2)
        std::int32_t RevokePowers(RE::StaticFunctionTag*) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                logger::warn("RevokePowers: Player not available");
                return 0;
            }

            auto* dh = RE::TESDataHandler::GetSingleton();
            auto* togglePower = dh ? dh->LookupForm<RE::SpellItem>(0x802, "CraftingInventoryExtender.esp") : nullptr;
            auto* detectPower = dh ? dh->LookupForm<RE::SpellItem>(0x804, "CraftingInventoryExtender.esp") : nullptr;

            std::int32_t removed = 0;

            if (togglePower && player->HasSpell(togglePower)) {
                player->RemoveSpell(togglePower);
                logger::info("RevokePowers: Removed SCIE_TogglePower from player");
                removed++;
            }

            if (detectPower && player->HasSpell(detectPower)) {
                player->RemoveSpell(detectPower);
                logger::info("RevokePowers: Removed SCIE_DetectPower from player");
                removed++;
            }

            return removed;
        }

        // ================================================================
        // Filtering Settings Functions (for MCM Filtering tab)
        // ================================================================

        /// Get a single filter setting
        /// stationType: 0=Crafting, 1=Tempering, 2=Enchanting, 3=Alchemy
        /// formType: 0=WEAP, 1=ARMO, 2=MISC, 3=INGR, 4=ALCH, 5=SLGM, 6=AMMO, 7=BOOK, 8=SCRL, 9=LIGH, 10=KEYM, 11=APPA
        /// Check if a filter combination is locked (cannot be enabled)
        bool IsFilterLocked(RE::StaticFunctionTag*, std::int32_t a_stationType, std::int32_t a_formType) {
            if (a_stationType < 0 || a_stationType >= static_cast<std::int32_t>(Services::StationType::COUNT) ||
                a_formType < 0 || a_formType >= static_cast<std::int32_t>(Services::FilterableFormType::COUNT)) {
                return false;
            }

            auto station = static_cast<Services::StationType>(a_stationType);
            auto formType = static_cast<Services::FilterableFormType>(a_formType);
            return Services::INISettings::IsFilterLocked(station, formType);
        }

        bool GetFilterSetting(RE::StaticFunctionTag*, std::int32_t a_stationType, std::int32_t a_formType) {
            if (a_stationType < 0 || a_stationType >= static_cast<std::int32_t>(Services::StationType::COUNT) ||
                a_formType < 0 || a_formType >= static_cast<std::int32_t>(Services::FilterableFormType::COUNT)) {
                return false;
            }

            auto station = static_cast<Services::StationType>(a_stationType);
            auto formType = static_cast<Services::FilterableFormType>(a_formType);
            return Services::INISettings::GetSingleton()->GetFilterSetting(station, formType);
        }

        /// Set a single filter setting and save to INI
        void SetFilterSetting(RE::StaticFunctionTag*, std::int32_t a_stationType, std::int32_t a_formType, bool a_enabled) {
            if (a_stationType < 0 || a_stationType >= static_cast<std::int32_t>(Services::StationType::COUNT) ||
                a_formType < 0 || a_formType >= static_cast<std::int32_t>(Services::FilterableFormType::COUNT)) {
                return;
            }

            auto station = static_cast<Services::StationType>(a_stationType);
            auto formType = static_cast<Services::FilterableFormType>(a_formType);
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetFilterSetting(station, formType, a_enabled);
            settings->Save();
        }

        /// Set all filters for a station (Select All / Select None buttons)
        void SetAllFilters(RE::StaticFunctionTag*, std::int32_t a_stationType, bool a_enabled) {
            if (a_stationType < 0 || a_stationType >= static_cast<std::int32_t>(Services::StationType::COUNT)) {
                return;
            }

            auto station = static_cast<Services::StationType>(a_stationType);
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetAllFilters(station, a_enabled);
            settings->Save();
        }

        /// Reset filter settings to defaults for a station (Defaults button)
        void ResetFilteringToDefaults(RE::StaticFunctionTag*, std::int32_t a_stationType) {
            if (a_stationType < 0 || a_stationType >= static_cast<std::int32_t>(Services::StationType::COUNT)) {
                return;
            }

            auto station = static_cast<Services::StationType>(a_stationType);
            auto* settings = Services::INISettings::GetSingleton();
            settings->ResetFilteringToDefaults(station);
            settings->Save();
        }

        /// Prepare for clean uninstall - revoke powers and clear all overrides
        /// Returns: 0 = success, negative = error
        std::int32_t PrepareForUninstall(RE::StaticFunctionTag*) {
            logger::info("PrepareForUninstall: Starting cleanup...");

            // Revoke powers from player
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto* dh = RE::TESDataHandler::GetSingleton();
                auto* togglePower = dh ? dh->LookupForm<RE::SpellItem>(0x802, "CraftingInventoryExtender.esp") : nullptr;
                auto* detectPower = dh ? dh->LookupForm<RE::SpellItem>(0x804, "CraftingInventoryExtender.esp") : nullptr;

                if (togglePower && player->HasSpell(togglePower)) {
                    player->RemoveSpell(togglePower);
                    logger::info("PrepareForUninstall: Removed SCIE_TogglePower");
                }
                if (detectPower && player->HasSpell(detectPower)) {
                    player->RemoveSpell(detectPower);
                    logger::info("PrepareForUninstall: Removed SCIE_DetectPower");
                }
            }

            // Clear all player overrides (globally, not just current cell)
            Services::ContainerRegistry::GetSingleton()->ClearPlayerOverrides();

            // Disable auto-grant so powers don't come back on reload
            auto* settings = Services::INISettings::GetSingleton();
            settings->SetAddPowersToPlayer(false);
            settings->Save();

            logger::info("PrepareForUninstall: Cleanup complete. Safe to uninstall.");
            return 0;
        }

        // =====================================================================
        // Translation functions
        // =====================================================================

        RE::BSFixedString Translate(RE::StaticFunctionTag*, RE::BSFixedString a_key) {
            return Services::TranslationService::GetSingleton()->GetTranslation(a_key.c_str());
        }

        RE::BSFixedString TranslateFormat(RE::StaticFunctionTag*, RE::BSFixedString a_key,
                                           RE::BSFixedString a_arg0, RE::BSFixedString a_arg1,
                                           RE::BSFixedString a_arg2) {
            return Services::TranslationService::GetSingleton()->FormatTranslation(
                a_key.c_str(), a_arg0.c_str(), a_arg1.c_str(), a_arg2.c_str());
        }
    }

    bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {
        if (!a_vm) {
            logger::error("Failed to register Papyrus functions: VM is null");
            return false;
        }

        // New functions
        a_vm->RegisterFunction("IsContainerUnsafe", ScriptName, IsContainerUnsafe);
        a_vm->RegisterFunction("GetContainerDisplayName", ScriptName, GetContainerDisplayName);
        a_vm->RegisterFunction("IsValidToggleTarget", ScriptName, IsValidToggleTarget);
        a_vm->RegisterFunction("ToggleCraftingContainer", ScriptName, ToggleCraftingContainer);
        a_vm->RegisterFunction("IsCraftingContainerActive", ScriptName, IsCraftingContainerActive);
        a_vm->RegisterFunction("GetContainerState", ScriptName, GetContainerState);

        // Debug/utility functions
        a_vm->RegisterFunction("DumpContainers", ScriptName, DumpContainers);

        // Legacy functions (still work, use new system internally)
        a_vm->RegisterFunction("RegisterCraftingContainer", ScriptName, RegisterCraftingContainer);
        a_vm->RegisterFunction("UnregisterCraftingContainer", ScriptName, UnregisterCraftingContainer);
        a_vm->RegisterFunction("IsContainerRegistered", ScriptName, IsContainerRegistered);
        a_vm->RegisterFunction("IsGlobalContainer", ScriptName, IsGlobalContainer);

        // Detect power functions
        a_vm->RegisterFunction("GetEnabledContainersInCell", ScriptName, GetEnabledContainersInCell);
        a_vm->RegisterFunction("GetDisabledContainersInCell", ScriptName, GetDisabledContainersInCell);
        a_vm->RegisterFunction("GetGlobalContainersInCell", ScriptName, GetGlobalContainersInCell);

        // Player container management (MCM Containers page)
        a_vm->RegisterFunction("GetPlayerContainerCount", ScriptName, GetPlayerContainerCount);
        a_vm->RegisterFunction("GetPlayerContainerNames", ScriptName, GetPlayerContainerNames);
        a_vm->RegisterFunction("GetPlayerContainerLocations", ScriptName, GetPlayerContainerLocations);
        a_vm->RegisterFunction("GetPlayerContainerStates", ScriptName, GetPlayerContainerStates);
        a_vm->RegisterFunction("SetPlayerContainerState", ScriptName, SetPlayerContainerState);
        a_vm->RegisterFunction("RemovePlayerContainersByLocation", ScriptName, RemovePlayerContainersByLocation);

        // MCM Configuration functions
        a_vm->RegisterFunction("GetMaxDistance", ScriptName, GetMaxDistance);
        a_vm->RegisterFunction("SetMaxDistance", ScriptName, SetMaxDistance);
        a_vm->RegisterFunction("GetModEnabled", ScriptName, GetModEnabled);
        a_vm->RegisterFunction("SetModEnabled", ScriptName, SetModEnabled);
        a_vm->RegisterFunction("GetDebugLogging", ScriptName, GetDebugLogging);
        a_vm->RegisterFunction("SetDebugLogging", ScriptName, SetDebugLogging);
        a_vm->RegisterFunction("GetEnableGlobalContainers", ScriptName, GetEnableGlobalContainers);
        a_vm->RegisterFunction("SetEnableGlobalContainers", ScriptName, SetEnableGlobalContainers);
        a_vm->RegisterFunction("GetEnableINIContainers", ScriptName, GetEnableINIContainers);
        a_vm->RegisterFunction("SetEnableINIContainers", ScriptName, SetEnableINIContainers);
        a_vm->RegisterFunction("GetEnableFollowerInventory", ScriptName, GetEnableFollowerInventory);
        a_vm->RegisterFunction("SetEnableFollowerInventory", ScriptName, SetEnableFollowerInventory);
        a_vm->RegisterFunction("GetAllowUnsafeContainers", ScriptName, GetAllowUnsafeContainers);
        a_vm->RegisterFunction("SetAllowUnsafeContainers", ScriptName, SetAllowUnsafeContainers);
        a_vm->RegisterFunction("IsLOTDInstalled", ScriptName, IsLOTDInstalled);
        a_vm->RegisterFunction("IsGeneralStoresInstalled", ScriptName, IsGeneralStoresInstalled);
        a_vm->RegisterFunction("IsLinkedCraftingStorageInstalled", ScriptName, IsLinkedCraftingStorageInstalled);
        a_vm->RegisterFunction("IsConvenientHorsesInstalled", ScriptName, IsConvenientHorsesInstalled);
        a_vm->RegisterFunction("IsHipBagInstalled", ScriptName, IsHipBagInstalled);
        a_vm->RegisterFunction("IsNFFInstalled", ScriptName, IsNFFInstalled);
        a_vm->RegisterFunction("IsEssentialFavoritesInstalled", ScriptName, IsEssentialFavoritesInstalled);
        a_vm->RegisterFunction("IsFavoriteMiscItemsInstalled", ScriptName, IsFavoriteMiscItemsInstalled);
        a_vm->RegisterFunction("GetFavoritedItemsExcludedCount", ScriptName, GetFavoritedItemsExcludedCount);
        a_vm->RegisterFunction("GetGlobalContainerCount", ScriptName, GetGlobalContainerCount);
        a_vm->RegisterFunction("GetTrackedContainerCount", ScriptName, GetTrackedContainerCount);

        // MCM Action functions
        a_vm->RegisterFunction("SnapshotToggles", ScriptName, SnapshotToggles);
        a_vm->RegisterFunction("ClearMarkedInCell", ScriptName, ClearMarkedInCell);

        // Power management functions (for mod authors)
        a_vm->RegisterFunction("GetAutoGrantPowers", ScriptName, GetAutoGrantPowers);
        a_vm->RegisterFunction("SetAutoGrantPowers", ScriptName, SetAutoGrantPowers);
        a_vm->RegisterFunction("GrantPowers", ScriptName, GrantPowers);
        a_vm->RegisterFunction("RevokePowers", ScriptName, RevokePowers);

        // Uninstall helper
        a_vm->RegisterFunction("PrepareForUninstall", ScriptName, PrepareForUninstall);

        // Translation functions
        a_vm->RegisterFunction("Translate", ScriptName, Translate);
        a_vm->RegisterFunction("TranslateFormat", ScriptName, TranslateFormat);

        // Filtering settings (MCM Filtering tab)
        a_vm->RegisterFunction("IsFilterLocked", ScriptName, IsFilterLocked);
        a_vm->RegisterFunction("GetFilterSetting", ScriptName, GetFilterSetting);
        a_vm->RegisterFunction("SetFilterSetting", ScriptName, SetFilterSetting);
        a_vm->RegisterFunction("SetAllFilters", ScriptName, SetAllFilters);
        a_vm->RegisterFunction("ResetFilteringToDefaults", ScriptName, ResetFilteringToDefaults);

        logger::info("Registered Papyrus native functions for {}", ScriptName);
        return true;
    }
}
