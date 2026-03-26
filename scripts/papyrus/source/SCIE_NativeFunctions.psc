Scriptname SCIE_NativeFunctions Hidden
{Native functions for Skyrim Crafting Inventory Extender.
 These functions are implemented in the DLL and can be called from any Papyrus script.

 FOR MOD AUTHORS:
 Use these functions to integrate SCIE with your mod. Common use cases:
 - Gate SCIE powers behind a quest (SetAutoGrantPowers + GrantPowers)
 - Check if a container is active before performing operations
 - Programmatically enable/disable containers

 All functions are Global Native - call them as:
   SCIE_NativeFunctions.FunctionName(args)}

; =============================================================================
; PLUGIN STATUS
; Check if the SKSE plugin (DLL) is actually loaded
; =============================================================================

; Check if the SCIE SKSE plugin is loaded.
; Returns true if the DLL is present and loaded by SKSE.
; Use this to detect installation problems before calling other native functions.
; NOTE: This is a pure Papyrus function, not native - it uses SKSE's plugin query.
bool Function IsPluginLoaded() Global
    return SKSE.GetPluginVersion("CraftingInventoryExtender") >= 0
EndFunction

; Get the SCIE plugin version.
; Returns the version number (e.g., 2 for v2.x), or -1 if not loaded.
int Function GetPluginVersion() Global
    return SKSE.GetPluginVersion("CraftingInventoryExtender")
EndFunction

; =============================================================================
; CONTAINER MANAGEMENT
; Functions for querying and modifying container states
; =============================================================================

; Check if a container has the respawn flag (unsafe for permanent storage).
; Checks both the base form (TESObjectCONT) and the placed reference flags.
; Returns true if the container's contents may reset periodically.
bool Function IsContainerUnsafe(ObjectReference akContainer) Global Native

; Check if a container is non-persistent (will be evicted when its cell unloads).
; Non-persistent containers cannot function as global containers because LookupByID
; returns null once the player leaves the area. Uses the engine's kRefOriginalPersistent flag.
bool Function IsContainerNonPersistent(ObjectReference akContainer) Global Native

; Get display name for a container, with warning suffixes if applicable.
; Appends " - UNSAFE" if the container respawns.
string Function GetContainerDisplayName(ObjectReference akContainer) Global Native

; Check if a reference is a valid target for the toggle power.
; Returns true for containers and player-owned mounts (horse saddlebags).
; Returns false for NPCs, plants, furniture, and other non-container objects.
bool Function IsValidToggleTarget(ObjectReference akRef) Global Native

; Toggle a container's crafting status through 3 states.
; Cycles: OFF -> LOCAL -> GLOBAL -> OFF
; Returns the new state: 0=off, 1=local, 2=global
; The change is saved to the cosave and persists across game sessions.
int Function ToggleCraftingContainer(ObjectReference akContainer) Global Native

; Get the effective state of a container (considers INI + player overrides).
; Returns: 0=off, 1=local, 2=global
int Function GetContainerState(ObjectReference akContainer) Global Native

; Check if a container is currently active for crafting.
; Returns true if the container will be used when a crafting menu opens.
; Takes into account: player overrides, INI configuration, ownership, locks.
bool Function IsCraftingContainerActive(ObjectReference akContainer) Global Native

; Get all containers in current cell that would be used for crafting.
; afMaxDistance: search radius in game units (0 = use INI default of 3000)
; Returns: array of ObjectReferences to enabled containers (local + global)
ObjectReference[] Function GetEnabledContainersInCell(float afMaxDistance = 0.0) Global Native

; Get all containers in current cell that were explicitly disabled by the player.
; These are containers that would otherwise be active (via INI) but player toggled off.
; afMaxDistance: search radius in game units (0 = use INI default)
ObjectReference[] Function GetDisabledContainersInCell(float afMaxDistance = 0.0) Global Native

; Get all containers in current cell that are in global state.
; afMaxDistance: search radius in game units (0 = use INI default)
ObjectReference[] Function GetGlobalContainersInCell(float afMaxDistance = 0.0) Global Native

; =============================================================================
; PLAYER CONTAINER MANAGEMENT (for MCM Containers page)
; =============================================================================

; Get count of player-overridden containers (cosave entries).
; These are containers the player has interacted with via toggle power.
int Function GetPlayerContainerCount() Global Native

; Get paginated container names (sorted by location then name).
; aiPage: 0-based page number
; aiPageSize: items per page (recommend 40 for MCM)
; Returns: array of container display names
string[] Function GetPlayerContainerNames(int aiPage, int aiPageSize) Global Native

; Get paginated container locations (parallel array with names).
; aiPage: 0-based page number
; aiPageSize: items per page
; Returns: array of location/cell names
string[] Function GetPlayerContainerLocations(int aiPage, int aiPageSize) Global Native

; Get paginated container states (parallel array with names).
; aiPage: 0-based page number
; aiPageSize: items per page
; Returns: array of states (0=off, 1=local, 2=global)
int[] Function GetPlayerContainerStates(int aiPage, int aiPageSize) Global Native

; Set a player container's state by sorted index (for MCM click-to-cycle).
; aiIndex: 0-based index into the full sorted list (page * pageSize + offset)
; aiNewState: 0=off, 1=local, 2=global
Function SetPlayerContainerState(int aiIndex, int aiNewState) Global Native

; Remove all player container overrides for a location (for MCM per-section clear button).
; Returns the number of overrides removed.
int Function RemovePlayerContainersByLocation(string asLocation) Global Native

; =============================================================================
; INI CONTAINER DISPLAY (for MCM Containers page)
; =============================================================================

; Get count of INI files that resolved at least one container.
; Returns 0 if no INI files are loaded or none resolved any containers.
int Function GetINISourceCount() Global Native

; Get INI source display name by index (0-based).
; Returns the filename without .ini extension (e.g., "SCIE_VanillaHomes").
string Function GetINISourceName(int aiIndex) Global Native

; Get container display names for an INI source, filtered by toggle state.
; asSourceName: display name from GetINISourceName
; abIncludeLocal: include [Containers] entries (respects bEnableINIContainers)
; abIncludeGlobal: include [GlobalContainers] entries (respects bEnableGlobalContainers)
; Returns: array of container display names with warning suffixes
string[] Function GetINISourceContainerNames(string asSourceName, bool abIncludeLocal, bool abIncludeGlobal) Global Native

; Get container states for an INI source (parallel array with names).
; Returns: array of states (1=local, 2=global)
int[] Function GetINISourceContainerStates(string asSourceName, bool abIncludeLocal, bool abIncludeGlobal) Global Native

; Get source plugin names for an INI source (parallel array with names).
; Returns: array of plugin filenames (e.g., "Skyrim.esm")
string[] Function GetINISourceContainerPlugins(string asSourceName, bool abIncludeLocal, bool abIncludeGlobal) Global Native

; Get cell/location names for an INI source (parallel array with names).
; Returns: array of cell display names (empty string if cell not loaded)
string[] Function GetINISourceContainerLocations(string asSourceName, bool abIncludeLocal, bool abIncludeGlobal) Global Native

; Get reachability for an INI source (parallel array with names).
; Returns: array of bools (true=reachable now, false=out of range/unloaded)
bool[] Function GetINISourceContainerReachable(string asSourceName, bool abIncludeLocal, bool abIncludeGlobal) Global Native

; =============================================================================
; LEGACY FUNCTIONS (for backwards compatibility)
; These still work but ToggleCraftingContainer is preferred
; =============================================================================

; Enable a container for crafting (no-op if already enabled)
Function RegisterCraftingContainer(ObjectReference akContainer) Global Native

; Disable a container for crafting (no-op if already disabled)
Function UnregisterCraftingContainer(ObjectReference akContainer) Global Native

; Check if container is registered (same as IsCraftingContainerActive)
bool Function IsContainerRegistered(ObjectReference akContainer) Global Native

; =============================================================================
; MCM CONFIGURATION FUNCTIONS
; These read/write the main INI file settings
; =============================================================================

; Get the max container search distance (INI: fMaxContainerDistance)
; Default: 3000 game units (covers a typical player home)
float Function GetMaxDistance() Global Native

; Set the max container search distance and save to INI
; Valid range: 100-10000 (clamped automatically)
Function SetMaxDistance(float afDistance) Global Native

; Get whether the mod is enabled (INI: bEnabled)
; When false, SCIE won't transfer items during crafting
bool Function GetModEnabled() Global Native

; Set whether the mod is enabled and save to INI
Function SetModEnabled(bool abEnabled) Global Native

; Log level constants (for aiLevel parameter):
;   0 = Info (normal operation: startup, session start/end, errors)
;   1 = Debug (troubleshooting: station detection, container resolution, summaries)
;   2 = Trace (extreme detail: every item query, per-hook entry - very spammy!)

; Get the current log level (0=Info, 1=Debug, 2=Trace)
int Function GetLogLevel() Global Native

; Set the log level and save to INI
; aiLevel: 0=Info, 1=Debug, 2=Trace
Function SetLogLevel(int aiLevel) Global Native

; BACKWARD COMPAT: Get whether debug logging is enabled (returns true if level >= Debug)
bool Function GetDebugLogging() Global Native

; BACKWARD COMPAT: Set debug logging (true sets Debug level, false sets Info level)
Function SetDebugLogging(bool abEnabled) Global Native

; Get count of active crafting containers in current cell within max distance
; Useful for status displays
int Function GetTrackedContainerCount() Global Native

; =============================================================================
; MOD AUTHOR UTILITIES
; Tools for creating container configurations
; =============================================================================

; Dump all containers in current cell to SKSE log.
; Output includes: plugin, FormID, EditorID, name, distance, lock/ownership status
; Console usage: cgf "SCIE_NativeFunctions.DumpContainers"
Function DumpContainers() Global Native

; Generate an INI file from player-marked containers in current cell.
; Only includes containers the player has manually enabled (not INI-configured).
; Returns: filepath to generated INI, or "" if no containers to snapshot
; Output location: Data/SKSE/Plugins/CraftingInventoryExtender/SCIE_Snapshot_*.ini
string Function SnapshotToggles() Global Native

; Clear all player overrides in current cell.
; Resets containers to their INI-configured state (or disabled if not in INI).
; Returns: count of overrides cleared
int Function ClearMarkedInCell() Global Native

; =============================================================================
; POWER MANAGEMENT (for mod authors)
; Control when players receive SCIE powers
; =============================================================================

; Get whether powers are auto-granted on game load (INI: bAddPowersToPlayer)
; Default: true
bool Function GetAutoGrantPowers() Global Native

; Set whether powers are auto-granted and save to INI.
; Set to false if you want to gate SCIE behind a quest or other condition.
Function SetAutoGrantPowers(bool abEnabled) Global Native

; Manually grant SCIE powers (Toggle and Detect) to the player.
; Returns: number of powers added (0 if already has both, 1 if had one, 2 if had none)
; Example usage: Grant powers when player completes a quest
int Function GrantPowers() Global Native

; Manually revoke SCIE powers from the player.
; Returns: number of powers removed (0-2)
; Example usage: Remove powers when a potion effect wears off
int Function RevokePowers() Global Native

; =============================================================================
; FOLLOWER INVENTORY
; =============================================================================

; Get whether follower/spouse inventory is included at crafting stations (INI: bEnableFollowerInventory)
; When enabled, nearby followers and spouses contribute safe item types during crafting.
; Weapons, armor, ammo are never taken. Potions/food only available at cooking stations.
; Default: true
bool Function GetEnableFollowerInventory() Global Native

; Set whether follower/spouse inventory is included and save to INI.
Function SetEnableFollowerInventory(bool abEnabled) Global Native

; Get whether unsafe (respawning) containers can be toggled (INI: bAllowUnsafeContainers)
; When false, the toggle power refuses to mark containers with the respawn flag.
; Default: false
bool Function GetAllowUnsafeContainers() Global Native

; Set whether unsafe containers can be toggled and save to INI.
Function SetAllowUnsafeContainers(bool abEnabled) Global Native

; =============================================================================
; INI CONTAINERS
; =============================================================================

; Get whether INI-configured containers are enabled (INI: bEnableINIContainers)
; When false, containers from INI preset files (player home configs) are ignored.
; Player-marked containers (cosave) and global containers still work.
; Default: true
bool Function GetEnableINIContainers() Global Native

; Set whether INI-configured containers are enabled and save to INI.
; Disable if you prefer to mark all containers yourself with the toggle power.
Function SetEnableINIContainers(bool abEnabled) Global Native

; =============================================================================
; GLOBAL CONTAINERS (for LOTD, General Stores, etc.)
; =============================================================================

; Get whether global containers are enabled (INI: bEnableGlobalContainers)
; When false, containers in [GlobalContainers] INI sections are ignored.
; Default: true
bool Function GetEnableGlobalContainers() Global Native

; Set whether global containers are enabled and save to INI.
; Use this to disable LOTD/General Stores global access if it conflicts with other mods.
Function SetEnableGlobalContainers(bool abEnabled) Global Native

; Get count of configured global containers (from all [GlobalContainers] INI sections + player promoted)
; Returns 0 if global containers are disabled or none are configured.
int Function GetGlobalContainerCount() Global Native

; Check if a specific container is configured as a global container.
; Global containers are accessible from any crafting station anywhere in the world.
; Use this for feedback in toggle scripts - global containers show special messages.
bool Function IsGlobalContainer(ObjectReference akContainer) Global Native

; =============================================================================
; MOD COMPATIBILITY DETECTION
; =============================================================================

; Check if Legacy of the Dragonborn is installed (LegacyoftheDragonborn.esm)
; Use this for informational display - LOTD is compatible with SCIE.
bool Function IsLOTDInstalled() Global Native

; Check if General Stores is installed (GeneralStores.esl)
; Use this for informational display - General Stores is compatible with SCIE.
bool Function IsGeneralStoresInstalled() Global Native

; Check if Linked Crafting Storage is installed (LinkedCraftingStorage.esp)
; WARNING: Linked Crafting Storage is INCOMPATIBLE with SCIE - both handle crafting materials.
; Use this to show a warning to users.
bool Function IsLinkedCraftingStorageInstalled() Global Native

; Check if Convenient Horses is installed (Convenient Horses.esp)
; When detected, SCIE also checks CHHorseFaction for horse ownership.
bool Function IsConvenientHorsesInstalled() Global Native

; Check if Hip Bag is installed (HipBag.esp)
; For informational display - Hip Bag global containers work with SCIE.
bool Function IsHipBagInstalled() Global Native

; Check if Nether's Follower Framework is installed (nwsFollowerFramework.esp)
; When detected, SCIE includes NFF "Additional Inventory" containers at crafting stations.
bool Function IsNFFInstalled() Global Native

; Check if Khajiit Will Follow is installed (KhajiitWillFollow.esp)
; When detected, SCIE includes KWF follower storage containers at crafting stations.
bool Function IsKWFInstalled() Global Native

; Check if Essential Favorites SKSE plugin is loaded (po3_EssentialFavorites.dll)
; SCIE respects favorited items regardless, but shows detection status in MCM.
bool Function IsEssentialFavoritesInstalled() Global Native

; Check if Favorite Misc Items SKSE plugin is loaded (po3_FavoriteMiscItems.dll)
; Works alongside Essential Favorites to allow favoriting MISC items.
bool Function IsFavoriteMiscItemsInstalled() Global Native

; Get count of item types currently excluded due to being favorited.
; This reflects the player's favorited items that SCIE is respecting.
; Only meaningful during an active crafting session; returns 0 otherwise.
int Function GetFavoritedItemsExcludedCount() Global Native

; =============================================================================
; SLID INTEGRATION
; Functions for integrating with SLID (Skyrim Linked Item Distribution)
; =============================================================================

; Check if SLID is installed (SLID.dll present)
bool Function IsSLIDInstalled() Global Native

; Request SLID to send updated network list
; Results are cached and available via GetSLIDNetworkCount/GetSLIDNetworkName
Function RefreshSLIDNetworks() Global Native

; Get count of available SLID networks
int Function GetSLIDNetworkCount() Global Native

; Get SLID network name by index (0-based)
string Function GetSLIDNetworkName(int aiIndex) Global Native

; Check if a SLID network is enabled for crafting
bool Function IsSLIDNetworkEnabled(string asNetworkName) Global Native

; Enable/disable a SLID network for crafting
; When enabled, containers in that network are included as crafting sources
Function SetSLIDNetworkEnabled(string asNetworkName, bool abEnabled) Global Native

; Get count of enabled SLID networks
int Function GetSLIDEnabledNetworkCount() Global Native

; Get count of missing SLID networks (enabled but no longer exist in SLID)
int Function GetSLIDMissingNetworkCount() Global Native

; Get missing SLID network name by index (0-based)
; Use with GetSLIDMissingNetworkCount to iterate missing networks
string Function GetSLIDMissingNetworkName(int aiIndex) Global Native

; Remove a SLID network from the enabled set
; Use this to clean up missing networks that no longer exist in SLID
Function RemoveSLIDNetwork(string asNetworkName) Global Native

; =============================================================================
; FILTERING SETTINGS (for MCM Filtering tab)
; Configure which form types are pulled from containers per station type
; =============================================================================

; Station type constants (for aiStationType parameter):
;   0 = Crafting (forge, smelter, tanning, cooking, staff enchanter, carpenter)
;   1 = Tempering (grindstone, armor workbench)
;   2 = Enchanting (arcane enchanter)
;   3 = Alchemy (alchemy lab)
;
; Form type constants (for aiFormType parameter):
;   0 = Weapons (WEAP)       6 = Ammunition (AMMO)
;   1 = Armor (ARMO)         7 = Books (BOOK)
;   2 = Miscellaneous (MISC) 8 = Scrolls (SCRL)
;   3 = Ingredients (INGR)   9 = Lights (LIGH)
;   4 = Potions/Food (ALCH)  10 = Keys (KEYM)
;   5 = Soul Gems (SLGM)     11 = Apparatus (APPA)

; Check if a filter combination is locked (cannot be enabled by the user).
; WEAP/ARMO at Tempering/Enchanting are locked because the game modifies these items
; in-place, which requires them to be in the player's inventory.
; aiStationType: 0-3 (see constants above)
; aiFormType: 0-11 (see constants above)
; Returns: true if the filter is locked off and cannot be enabled
bool Function IsFilterLocked(int aiStationType, int aiFormType) Global Native

; Get whether a specific form type is pulled from containers for a station.
; aiStationType: 0-3 (see constants above)
; aiFormType: 0-11 (see constants above)
; Returns: true if that form type will be pulled from containers at that station
bool Function GetFilterSetting(int aiStationType, int aiFormType) Global Native

; Set whether a specific form type is pulled from containers for a station.
; Changes are saved to INI immediately.
Function SetFilterSetting(int aiStationType, int aiFormType, bool abEnabled) Global Native

; Set all filters for a station to the same value.
; Use for "Select All" (true) and "Select None" (false) buttons.
Function SetAllFilters(int aiStationType, bool abEnabled) Global Native

; Reset filter settings to curated defaults for a station.
; Defaults vary per station type (see docs/feature_plans/mcm_filtering.md).
Function ResetFilteringToDefaults(int aiStationType) Global Native

; =============================================================================
; TRANSLATION FUNCTIONS
; Look up strings from the shared translation file
; (Data/Interface/Translations/CraftingInventoryExtender_LANGUAGE.txt)
; =============================================================================

; Look up a translation key from the translation file.
; asKey: Translation key (e.g., "$SCIE_ErrNoTarget")
; Returns: Translated text, or the key itself if not found.
string Function Translate(string asKey) Global Native

; Look up a translation key and replace {0}, {1}, {2} placeholders with args.
; Translators can reorder placeholders for grammar differences between languages.
; asKey: Translation key (e.g., "$SCIE_NotifyStateOff")
; asArg0-2: Values to substitute for {0}, {1}, {2} (default: "")
; Returns: Formatted translated string.
string Function TranslateFormat(string asKey, string asArg0 = "", string asArg1 = "", string asArg2 = "") Global Native

; =============================================================================
; UNINSTALL HELPER
; =============================================================================

; Prepare the mod for clean uninstallation.
; This function:
;   1. Revokes all SCIE powers from the player
;   2. Clears ALL player container overrides (globally, not just current cell)
;   3. Disables auto-grant powers so they don't come back on reload
; After calling this, save the game and then remove the mod files.
; Returns: 0 on success
int Function PrepareForUninstall() Global Native
