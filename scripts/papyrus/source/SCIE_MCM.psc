Scriptname SCIE_MCM extends SKI_ConfigBase
{MCM configuration script for Skyrim Crafting Inventory Extender.
 Provides in-game configuration of mod settings via SkyUI's MCM framework.

 FOR MOD AUTHORS:
 This script demonstrates how to integrate with SCIE's native functions.
 All settings are read from and written to the DLL via SCIE_NativeFunctions.
 See SCIE_NativeFunctions.psc for the full API documentation.

 SETUP REQUIREMENTS:
 1. Add SCIE_MCM quest to your ESP (Start Game Enabled, Run Once = false)
 2. Attach this script to the quest
 3. Compile with SkyUI SDK's SKI_ConfigBase.psc in your source path

 TRANSLATION:
 All user-visible strings use $SCIE_ prefixed keys for SkyUI's translation system.
 Translators: edit Data/Interface/Translations/CraftingInventoryExtender_<LANGUAGE>.txt
 See docs/Translation_Plan.md for details.}

; =============================================================================
; HOW THIS MCM WORKS
; =============================================================================
; All configuration is stored in the DLL, not in Papyrus variables.
; Native functions in SCIE_NativeFunctions handle the read/write operations.
; Changes are immediately saved to the INI file by the DLL.
;
; Call pattern for all settings:
;   Read:  SCIE_NativeFunctions.GetSomething()
;   Write: SCIE_NativeFunctions.SetSomething(value)  ; auto-saves to INI
; =============================================================================

; =============================================================================
; Constants
; =============================================================================

; Page names ($ prefix = SkyUI auto-translates from translation file)
string Property PAGE_SETTINGS = "$SCIE_PageSettings" AutoReadOnly
string Property PAGE_CONTAINERS = "$SCIE_PageContainers" AutoReadOnly
string Property PAGE_FILTERING = "$SCIE_PageFiltering" AutoReadOnly
string Property PAGE_COMPATIBILITY = "$SCIE_PageCompatibility" AutoReadOnly
string Property PAGE_MAINTENANCE = "$SCIE_PageMaintenance" AutoReadOnly
string Property PAGE_MODAUTHOR = "$SCIE_PageModAuthor" AutoReadOnly
string Property PAGE_ABOUT = "$SCIE_PageAbout" AutoReadOnly

; Slider bounds for distance
float Property DISTANCE_MIN = 500.0 AutoReadOnly
float Property DISTANCE_MAX = 5000.0 AutoReadOnly
float Property DISTANCE_STEP = 100.0 AutoReadOnly
float Property DISTANCE_DEFAULT = 3000.0 AutoReadOnly

; Containers page pagination
int Property CONTAINERS_PAGE_SIZE = 40 AutoReadOnly


; =============================================================================
; Option IDs (populated in OnPageReset)
; =============================================================================

; Settings page
int oidDistanceSlider
int oidTrackedText
int oidClearCellButton
int oidGlobalContainersToggle
int oidGlobalCountText
int oidINIContainersToggle
int oidFollowerInventoryToggle
int oidAllowUnsafeToggle

; Containers page
int[] oidContainerOptions      ; Option IDs for container text options
int[] containerGlobalIndices   ; Maps option index -> global sorted index for SetPlayerContainerState
int containersPageNum = 0      ; Current page (0-based)
int oidContainersPrevButton
int oidContainersNextButton
int[] oidLocationClearButtons  ; Option IDs for per-location clear buttons
string[] locationClearNames    ; Location names for each clear button

; Maintenance page
int oidModEnabledToggle
int oidAutoGrantPowersToggle
int oidGrantPowersButton
int oidRevokePowersButton
int oidLogLevelMenu
int oidUninstallButton

; Log level menu options
string[] logLevelNames

; Mod Author Tools page
int oidScanButton
int oidSnapshotButton

; Filtering page
int oidStationMenu              ; Dropdown for station type selection
int oidSelectAllButton          ; Select All button
int oidSelectNoneButton         ; Select None button
int oidDefaultsButton           ; Defaults button
int[] oidFilterToggles          ; Array of 12 toggle option IDs (WEAP, ARMO, MISC, etc.)
int selectedStation = 0         ; Currently selected station (0=Crafting, 1=Tempering, 2=Enchanting, 3=Alchemy)

; Form type labels, record codes, and info text for filtering toggles
string[] formTypeLabels
string[] formTypeCodes
string[] formTypeInfoTexts

; Station names for filtering dropdown
string[] stationNames

; SLID Integration
int[] oidSLIDNetworkToggles      ; Array of toggle option IDs for SLID networks
string[] slidNetworkNames         ; Cached network names for toggle handling
int slidNetworkCount = 0          ; Number of networks currently displayed
int[] oidSLIDMissingRemove       ; Array of remove button OIDs for missing networks
string[] slidMissingNames        ; Cached missing network names
int slidMissingCount = 0         ; Number of missing networks displayed

; =============================================================================
; MCM Setup
; =============================================================================

Event OnConfigInit()
    ModName = "SCIE"
    CurrentVersion = 258  ; v2.5.8: Retire recipe patch ESPs, Hook 5 defense-in-depth
    InitializePages()
    InitializeFilteringArrays()
EndEvent

Event OnVersionUpdate(int a_version)
    ; Called by SkyUI when CurrentVersion increases - allows updating existing saves
    InitializePages()
    InitializeFilteringArrays()
EndEvent

Function InitializePages()
    Pages = new string[7]
    Pages[0] = PAGE_SETTINGS
    Pages[1] = PAGE_CONTAINERS
    Pages[2] = PAGE_FILTERING
    Pages[3] = PAGE_COMPATIBILITY
    Pages[4] = PAGE_MAINTENANCE
    Pages[5] = PAGE_MODAUTHOR
    Pages[6] = PAGE_ABOUT
EndFunction

Function InitializeFilteringArrays()
    oidFilterToggles = new int[12]

    ; SLID network arrays (max 10 networks shown in MCM)
    oidSLIDNetworkToggles = new int[10]
    slidNetworkNames = new string[10]
    oidSLIDMissingRemove = new int[10]
    slidMissingNames = new string[10]

    formTypeLabels = new string[12]
    formTypeLabels[0] = "$SCIE_FormWeapons"
    formTypeLabels[1] = "$SCIE_FormArmor"
    formTypeLabels[2] = "$SCIE_FormMisc"
    formTypeLabels[3] = "$SCIE_FormIngredients"
    formTypeLabels[4] = "$SCIE_FormPotions"
    formTypeLabels[5] = "$SCIE_FormSoulGems"
    formTypeLabels[6] = "$SCIE_FormAmmo"
    formTypeLabels[7] = "$SCIE_FormBooks"
    formTypeLabels[8] = "$SCIE_FormScrolls"
    formTypeLabels[9] = "$SCIE_FormLights"
    formTypeLabels[10] = "$SCIE_FormKeys"
    formTypeLabels[11] = "$SCIE_FormApparatus"

    formTypeCodes = new string[12]
    formTypeCodes[0] = "(WEAP)"
    formTypeCodes[1] = "(ARMO)"
    formTypeCodes[2] = "(MISC)"
    formTypeCodes[3] = "(INGR)"
    formTypeCodes[4] = "(ALCH)"
    formTypeCodes[5] = "(SLGM)"
    formTypeCodes[6] = "(AMMO)"
    formTypeCodes[7] = "(BOOK)"
    formTypeCodes[8] = "(SCRL)"
    formTypeCodes[9] = "(LIGH)"
    formTypeCodes[10] = "(KEYM)"
    formTypeCodes[11] = "(APPA)"

    formTypeInfoTexts = new string[12]
    formTypeInfoTexts[0] = "$SCIE_InfoFilterWEAP"
    formTypeInfoTexts[1] = "$SCIE_InfoFilterARMO"
    formTypeInfoTexts[2] = "$SCIE_InfoFilterMISC"
    formTypeInfoTexts[3] = "$SCIE_InfoFilterINGR"
    formTypeInfoTexts[4] = "$SCIE_InfoFilterALCH"
    formTypeInfoTexts[5] = "$SCIE_InfoFilterSLGM"
    formTypeInfoTexts[6] = "$SCIE_InfoFilterAMMO"
    formTypeInfoTexts[7] = "$SCIE_InfoFilterBOOK"
    formTypeInfoTexts[8] = "$SCIE_InfoFilterSCRL"
    formTypeInfoTexts[9] = "$SCIE_InfoFilterLIGH"
    formTypeInfoTexts[10] = "$SCIE_InfoFilterKEYM"
    formTypeInfoTexts[11] = "$SCIE_InfoFilterAPPA"

    stationNames = new string[4]
    stationNames[0] = "$SCIE_StationCrafting"
    stationNames[1] = "$SCIE_StationTempering"
    stationNames[2] = "$SCIE_StationEnchanting"
    stationNames[3] = "$SCIE_StationAlchemy"

    logLevelNames = new string[3]
    logLevelNames[0] = "$SCIE_LogLevelInfo"
    logLevelNames[1] = "$SCIE_LogLevelDebug"
    logLevelNames[2] = "$SCIE_LogLevelTrace"
EndFunction

Function ResetOptionIDs()
    ; Reset all option IDs to -1 (invalid) to prevent cross-page collisions
    ; Settings page
    oidDistanceSlider = -1
    oidTrackedText = -1
    oidClearCellButton = -1
    oidGlobalContainersToggle = -1
    oidGlobalCountText = -1
    oidINIContainersToggle = -1
    oidFollowerInventoryToggle = -1
    oidAllowUnsafeToggle = -1

    ; Containers page
    oidContainersPrevButton = -1
    oidContainersNextButton = -1

    ; Maintenance page
    oidModEnabledToggle = -1
    oidAutoGrantPowersToggle = -1
    oidGrantPowersButton = -1
    oidRevokePowersButton = -1
    oidLogLevelMenu = -1
    oidUninstallButton = -1

    ; Mod Author Tools page
    oidScanButton = -1
    oidSnapshotButton = -1

    ; Filtering page
    oidStationMenu = -1
    oidSelectAllButton = -1
    oidSelectNoneButton = -1
    oidDefaultsButton = -1
    int i = 0
    while i < 12
        oidFilterToggles[i] = -1
        i += 1
    endwhile
EndFunction

; =============================================================================
; Page Rendering
; =============================================================================

Event OnPageReset(string page)
    ; Always ensure pages are initialized (handles existing saves that missed OnVersionUpdate)
    if Pages.Length != 7 || !stationNames || stationNames.Length != 4 || !formTypeInfoTexts || formTypeInfoTexts.Length != 12 || !logLevelNames || logLevelNames.Length != 3
        InitializePages()
        InitializeFilteringArrays()
    endif

    ; Reset all option IDs to prevent cross-page collisions
    ResetOptionIDs()

    SetCursorFillMode(TOP_TO_BOTTOM)

    ; Check if the SKSE plugin is loaded - this must come first
    if !SCIE_NativeFunctions.IsPluginLoaded()
        RenderPluginNotLoadedPage()
        return
    endif

    ; NOTE: OnPageReset receives the TRANSLATED page name (e.g., "Settings"),
    ; not the raw $KEY. Compare against Pages[] which SkyUI also translates.
    if page == "" || page == Pages[0]
        RenderSettingsPage()
    elseif page == Pages[1]
        RenderContainersPage()
    elseif page == Pages[2]
        RenderFilteringPage()
    elseif page == Pages[3]
        RenderCompatibilityPage()
    elseif page == Pages[4]
        RenderMaintenancePage()
    elseif page == Pages[5]
        RenderModAuthorPage()
    elseif page == Pages[6]
        RenderAboutPage()
    endif
EndEvent

Function RenderSettingsPage()
    SetCursorPosition(0)

    ; Header
    AddHeaderOption("$SCIE_HeaderContainerDetection")

    ; Distance slider
    float currentDistance = SCIE_NativeFunctions.GetMaxDistance()
    oidDistanceSlider = AddSliderOption("$SCIE_LabelScanDistance", currentDistance, "$SCIE_FmtUnits")

    ; Tracked container count (read-only display)
    int trackedCount = SCIE_NativeFunctions.GetTrackedContainerCount()
    oidTrackedText = AddTextOption("$SCIE_LabelTrackedContainers", "$SCIE_FmtInRange{" + trackedCount + "}")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderGlobalContainers")

    ; Global containers toggle
    bool globalEnabled = SCIE_NativeFunctions.GetEnableGlobalContainers()
    oidGlobalContainersToggle = AddToggleOption("$SCIE_LabelEnableGlobal", globalEnabled)

    ; Global container count (read-only display)
    int globalCount = SCIE_NativeFunctions.GetGlobalContainerCount()
    oidGlobalCountText = AddTextOption("$SCIE_LabelConfigured", "$SCIE_FmtContainerCount{" + globalCount + "}")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderINIContainers")

    ; INI containers toggle
    bool iniEnabled = SCIE_NativeFunctions.GetEnableINIContainers()
    oidINIContainersToggle = AddToggleOption("$SCIE_LabelEnableINI", iniEnabled)

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderFollowerInventory")

    ; Follower inventory toggle
    bool followerEnabled = SCIE_NativeFunctions.GetEnableFollowerInventory()
    oidFollowerInventoryToggle = AddToggleOption("$SCIE_LabelIncludeFollowers", followerEnabled)

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderContainerSafety")

    ; Allow unsafe containers toggle
    bool allowUnsafe = SCIE_NativeFunctions.GetAllowUnsafeContainers()
    oidAllowUnsafeToggle = AddToggleOption("$SCIE_LabelAllowUnsafe", allowUnsafe)

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderCellManagement")

    ; Clear marked in cell button
    oidClearCellButton = AddTextOption("$SCIE_LabelClearMarked", "$SCIE_ValClear")
EndFunction

Function RenderContainersPage()
    SetCursorPosition(0)

    int totalCount = SCIE_NativeFunctions.GetPlayerContainerCount()

    if totalCount == 0
        AddHeaderOption("$SCIE_HeaderPlayerContainers")
        AddTextOption("", "$SCIE_ContainersNone", OPTION_FLAG_DISABLED)
        AddTextOption("", "$SCIE_ContainersHint1", OPTION_FLAG_DISABLED)
        AddTextOption("", "$SCIE_ContainersHint2", OPTION_FLAG_DISABLED)
        return
    endif

    ; Calculate pagination
    int totalPages = ((totalCount - 1) / CONTAINERS_PAGE_SIZE) + 1
    if containersPageNum >= totalPages
        containersPageNum = totalPages - 1
    endif

    ; Fetch page data
    string[] names = SCIE_NativeFunctions.GetPlayerContainerNames(containersPageNum, CONTAINERS_PAGE_SIZE)
    string[] locations = SCIE_NativeFunctions.GetPlayerContainerLocations(containersPageNum, CONTAINERS_PAGE_SIZE)
    int[] states = SCIE_NativeFunctions.GetPlayerContainerStates(containersPageNum, CONTAINERS_PAGE_SIZE)

    int pageItemCount = names.Length

    ; Header with page info
    if totalPages > 1
        AddHeaderOption("$SCIE_FmtPageHeader{" + (containersPageNum + 1) + "}_Of{" + totalPages + "}")
    else
        AddHeaderOption("$SCIE_FmtMarkedHeader{" + totalCount + "}")
    endif

    ; Allocate tracking arrays
    oidContainerOptions = new int[128]
    containerGlobalIndices = new int[128]
    oidLocationClearButtons = new int[32]
    locationClearNames = new string[32]
    int optionIdx = 0
    int clearIdx = 0

    ; Render containers grouped by location
    string currentLocation = ""
    int i = 0
    while i < pageItemCount && optionIdx < 120  ; Leave room for nav buttons
        ; Add location header when location changes
        if locations[i] != currentLocation
            currentLocation = locations[i]
            AddHeaderOption(currentLocation)
            ; Add clear button in right column next to header
            if clearIdx < 32
                oidLocationClearButtons[clearIdx] = AddTextOption("", "$SCIE_ValClear")
                locationClearNames[clearIdx] = currentLocation
                clearIdx += 1
            endif
        endif

        ; Get state label
        string stateLabel
        if states[i] == 0
            stateLabel = "$SCIE_StateOff"
        elseif states[i] == 1
            stateLabel = "$SCIE_StateLocal"
        elseif states[i] == 2
            stateLabel = "$SCIE_StateGlobal"
        else
            stateLabel = "$SCIE_StateUnknown"
        endif

        ; Add clickable text option
        oidContainerOptions[optionIdx] = AddTextOption(names[i], stateLabel)
        containerGlobalIndices[optionIdx] = (containersPageNum * CONTAINERS_PAGE_SIZE) + i
        optionIdx += 1

        i += 1
    endwhile

    ; Navigation buttons
    if totalPages > 1
        AddEmptyOption()
        AddHeaderOption("$SCIE_HeaderNavigation")
        if containersPageNum > 0
            oidContainersPrevButton = AddTextOption("$SCIE_LabelPrevPage", "<<")
        else
            oidContainersPrevButton = AddTextOption("$SCIE_LabelPrevPage", "<<", OPTION_FLAG_DISABLED)
        endif
        if containersPageNum < totalPages - 1
            oidContainersNextButton = AddTextOption("$SCIE_LabelNextPage", ">>")
        else
            oidContainersNextButton = AddTextOption("$SCIE_LabelNextPage", ">>", OPTION_FLAG_DISABLED)
        endif
    endif
EndFunction

Function RenderFilteringPage()
    SetCursorPosition(0)

    ; Header and station selector
    AddHeaderOption("$SCIE_HeaderStationType")

    ; Station type dropdown menu
    oidStationMenu = AddMenuOption("$SCIE_LabelStation", stationNames[selectedStation])

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderFormTypes")

    ; Add toggles for all 12 form types in 2 columns (6 rows)
    ; Use LEFT_TO_RIGHT fill mode for automatic column alternation
    SetCursorFillMode(LEFT_TO_RIGHT)
    int i = 0
    while i < 12
        bool enabled = SCIE_NativeFunctions.GetFilterSetting(selectedStation, i)
        bool locked = SCIE_NativeFunctions.IsFilterLocked(selectedStation, i)
        if locked
            oidFilterToggles[i] = AddToggleOption(formTypeLabels[i], false, OPTION_FLAG_DISABLED)
        else
            oidFilterToggles[i] = AddToggleOption(formTypeLabels[i], enabled)
        endif
        i += 1
    endwhile

    ; Bulk action buttons at bottom
    SetCursorFillMode(TOP_TO_BOTTOM)
    AddEmptyOption()
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderQuickActions")
    oidSelectAllButton = AddTextOption("$SCIE_LabelSelectAll", "$SCIE_ValApply")
    oidSelectNoneButton = AddTextOption("$SCIE_LabelSelectNone", "$SCIE_ValApply")
    oidDefaultsButton = AddTextOption("$SCIE_LabelResetDefaults", "$SCIE_ValApply")
EndFunction

Function RenderCompatibilityPage()
    SetCursorPosition(0)

    ; Global Storage Mods
    AddHeaderOption("$SCIE_HeaderGlobalStorageMods")

    bool hasLOTD = SCIE_NativeFunctions.IsLOTDInstalled()
    bool hasGS = SCIE_NativeFunctions.IsGeneralStoresInstalled()
    bool hasHipBag = SCIE_NativeFunctions.IsHipBagInstalled()
    bool globalEnabled = SCIE_NativeFunctions.GetEnableGlobalContainers()

    if hasLOTD
        if globalEnabled
            AddTextOption("$SCIE_CompatLOTD", "$SCIE_ValDetected")
        else
            AddTextOption("$SCIE_CompatLOTD", "$SCIE_ValDetectedGlobalsOff")
        endif
    else
        AddTextOption("$SCIE_CompatLOTD", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    if hasGS
        if globalEnabled
            AddTextOption("$SCIE_CompatGS", "$SCIE_ValDetected")
        else
            AddTextOption("$SCIE_CompatGS", "$SCIE_ValDetectedGlobalsOff")
        endif
    else
        AddTextOption("$SCIE_CompatGS", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    if hasHipBag
        if globalEnabled
            AddTextOption("$SCIE_CompatHipBag", "$SCIE_ValDetected")
        else
            AddTextOption("$SCIE_CompatHipBag", "$SCIE_ValDetectedGlobalsOff")
        endif
    else
        AddTextOption("$SCIE_CompatHipBag", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    if hasLOTD && globalEnabled
        AddEmptyOption()
        AddTextOption("$SCIE_TipDisableCraftLoot", "", OPTION_FLAG_DISABLED)
    endif

    ; Horse Mods
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderHorseMods")

    bool hasCH = SCIE_NativeFunctions.IsConvenientHorsesInstalled()
    if hasCH
        AddTextOption("$SCIE_CompatCH", "$SCIE_ValDetectedCHFaction")
    else
        AddTextOption("$SCIE_CompatCH", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif
    AddTextOption("$SCIE_CompatVanillaHorses", "$SCIE_ValAlwaysSupported")

    ; Follower Support
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderFollowerSupport")

    bool followerEnabled = SCIE_NativeFunctions.GetEnableFollowerInventory()
    if followerEnabled
        AddTextOption("$SCIE_CompatFollowerInv", "$SCIE_ValEnabled")
    else
        AddTextOption("$SCIE_CompatFollowerInv", "$SCIE_ValDisabled")
    endif

    bool hasNFF = SCIE_NativeFunctions.IsNFFInstalled()
    if hasNFF
        AddTextOption("$SCIE_CompatNFF", "$SCIE_ValDetected")
        AddTextOption("$SCIE_CompatNFFAdditional", "$SCIE_ValSupported", OPTION_FLAG_DISABLED)
    else
        AddTextOption("$SCIE_CompatNFF", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    bool hasKWF = SCIE_NativeFunctions.IsKWFInstalled()
    if hasKWF
        AddTextOption("$SCIE_CompatKWF", "$SCIE_ValDetected")
        AddTextOption("$SCIE_CompatKWFStorage", "$SCIE_ValSupported", OPTION_FLAG_DISABLED)
    else
        AddTextOption("$SCIE_CompatKWF", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    AddTextOption("$SCIE_CompatSafetyFilter", "$SCIE_ValSafetyDesc", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_CompatCookingException", "$SCIE_ValCookingDesc", OPTION_FLAG_DISABLED)

    ; SLID Integration
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderSLIDIntegration")

    bool hasSLID = SCIE_NativeFunctions.IsSLIDInstalled()
    if hasSLID
        ; Request fresh network list
        SCIE_NativeFunctions.RefreshSLIDNetworks()

        AddTextOption("$SCIE_CompatSLID", "$SCIE_ValDetected")

        int networkCount = SCIE_NativeFunctions.GetSLIDNetworkCount()
        int missingCount = SCIE_NativeFunctions.GetSLIDMissingNetworkCount()

        ; Show available networks with toggles
        if networkCount > 0
            AddTextOption("$SCIE_SLIDNetworksAvailable", networkCount, OPTION_FLAG_DISABLED)

            int i = 0
            while i < networkCount && i < 10
                string networkName = SCIE_NativeFunctions.GetSLIDNetworkName(i)
                bool isEnabled = SCIE_NativeFunctions.IsSLIDNetworkEnabled(networkName)
                oidSLIDNetworkToggles[i] = AddToggleOption(networkName, isEnabled)
                slidNetworkNames[i] = networkName
                i += 1
            endwhile
            slidNetworkCount = i

            if networkCount > 10
                AddTextOption("$SCIE_SLIDMoreNetworks", "", OPTION_FLAG_DISABLED)
            endif
        else
            AddTextOption("$SCIE_SLIDNoNetworks", "", OPTION_FLAG_DISABLED)
            slidNetworkCount = 0
        endif

        ; Show missing networks with Remove buttons
        if missingCount > 0
            AddEmptyOption()
            AddTextOption("$SCIE_SLIDMissingNetworks", missingCount, OPTION_FLAG_DISABLED)

            int j = 0
            while j < missingCount && j < 10
                string missingName = SCIE_NativeFunctions.GetSLIDMissingNetworkName(j)
                ; Show greyed out name on left, Remove button on right
                AddTextOption(missingName, "", OPTION_FLAG_DISABLED)
                oidSLIDMissingRemove[j] = AddTextOption("", "$SCIE_SLIDRemove")
                slidMissingNames[j] = missingName
                j += 1
            endwhile
            slidMissingCount = j
        else
            slidMissingCount = 0
        endif
    else
        AddTextOption("$SCIE_CompatSLID", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
        slidNetworkCount = 0
        slidMissingCount = 0
    endif

    ; Item Protection
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderItemProtection")

    bool hasEF = SCIE_NativeFunctions.IsEssentialFavoritesInstalled()
    bool hasFMI = SCIE_NativeFunctions.IsFavoriteMiscItemsInstalled()

    if hasEF
        AddTextOption("$SCIE_CompatEF", "$SCIE_ValDetected")
    else
        AddTextOption("$SCIE_CompatEF", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    if hasFMI
        AddTextOption("$SCIE_CompatFMI", "$SCIE_ValDetected")
    else
        AddTextOption("$SCIE_CompatFMI", "$SCIE_ValNotInstalled", OPTION_FLAG_DISABLED)
    endif

    AddTextOption("$SCIE_CompatFavoritesExcluded", "$SCIE_ValAlwaysRespected")

    ; Incompatible Mods
    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderIncompatibleMods")

    bool hasLCS = SCIE_NativeFunctions.IsLinkedCraftingStorageInstalled()
    if hasLCS
        AddTextOption("$SCIE_CompatLCS", "$SCIE_ValConflict")
        AddTextOption("$SCIE_WarnBothHandle", "", OPTION_FLAG_DISABLED)
        AddTextOption("$SCIE_WarnChooseOne", "", OPTION_FLAG_DISABLED)
    else
        AddTextOption("$SCIE_ValNoConflicts", "", OPTION_FLAG_DISABLED)
    endif
EndFunction

Function RenderMaintenancePage()
    SetCursorPosition(0)

    ; Header
    AddHeaderOption("$SCIE_HeaderModControl")

    ; Mod enabled toggle
    bool modEnabled = SCIE_NativeFunctions.GetModEnabled()
    oidModEnabledToggle = AddToggleOption("$SCIE_LabelModEnabled", modEnabled)

    ; Log level menu
    int currentLogLevel = SCIE_NativeFunctions.GetLogLevel()
    oidLogLevelMenu = AddMenuOption("$SCIE_LabelLogLevel", logLevelNames[currentLogLevel])

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderPowers")

    ; Auto-grant powers toggle (controls game load behavior only)
    bool autoGrant = SCIE_NativeFunctions.GetAutoGrantPowers()
    oidAutoGrantPowersToggle = AddToggleOption("$SCIE_LabelAutoGrant", autoGrant)

    ; Manual grant/revoke buttons
    oidGrantPowersButton = AddTextOption("$SCIE_LabelGrantPowers", "$SCIE_ValGrant")
    oidRevokePowersButton = AddTextOption("$SCIE_LabelRevokePowers", "$SCIE_ValRevoke")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderUninstall")

    ; Prepare for uninstall button
    oidUninstallButton = AddTextOption("$SCIE_LabelPrepareUninstall", "$SCIE_ValRun")
EndFunction

Function RenderModAuthorPage()
    SetCursorPosition(0)

    ; Header
    AddHeaderOption("$SCIE_HeaderTools")

    ; Scan current cell button
    oidScanButton = AddTextOption("$SCIE_LabelScanCell", "$SCIE_ValRun")

    ; Snapshot toggles button
    oidSnapshotButton = AddTextOption("$SCIE_LabelSnapshot", "$SCIE_ValGenerateINI")
EndFunction

Function RenderAboutPage()
    SetCursorPosition(0)

    AddHeaderOption("$SCIE_HeaderSCIE")
    AddTextOption("$SCIE_LabelVersion", "2.5.8")
    AddTextOption("$SCIE_LabelAuthor", "Ohfor")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderDescription")
    AddTextOption("", "$SCIE_DescLine1")
    AddTextOption("", "$SCIE_DescLine2")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderUsage")
    AddTextOption("", "$SCIE_UsageLine1")
    AddTextOption("", "$SCIE_UsageLine2")
    AddTextOption("", "$SCIE_UsageLine3")

    AddEmptyOption()
    AddHeaderOption("$SCIE_HeaderCredits")
    AddTextOption("", "$SCIE_CreditCobb")
    AddTextOption("", "$SCIE_CreditJerry")
    AddTextOption("", "$SCIE_CreditBaryon")
EndFunction

Function RenderPluginNotLoadedPage()
    ; Show a prominent error when the SKSE plugin isn't loaded
    SetCursorPosition(0)

    AddHeaderOption("$SCIE_ErrHeader")
    AddEmptyOption()
    AddTextOption("$SCIE_ErrNotLoaded", "", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_ErrWontWork", "", OPTION_FLAG_DISABLED)
    AddEmptyOption()

    AddHeaderOption("$SCIE_ErrTroubleshooting")
    AddTextOption("$SCIE_ErrStep1Label", "$SCIE_ErrStep1Value", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_ErrStep2Label", "$SCIE_ErrStep2Value", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_ErrStep3Label", "$SCIE_ErrStep3Value", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_ErrStep4Label", "$SCIE_ErrStep4Value", OPTION_FLAG_DISABLED)
    AddEmptyOption()

    AddHeaderOption("$SCIE_ErrFilesRequired")
    AddTextOption("$SCIE_ErrDLLLabel", "$SCIE_ErrDLLPath", OPTION_FLAG_DISABLED)
    AddTextOption("$SCIE_ErrAddrLabel", "$SCIE_ErrAddrPath", OPTION_FLAG_DISABLED)
EndFunction

; =============================================================================
; Option Handlers
; =============================================================================

Event OnOptionHighlight(int option)
    if option == oidDistanceSlider
        SetInfoText("$SCIE_InfoScanDistance")
    elseif option == oidTrackedText
        SetInfoText("$SCIE_InfoTrackedCount")
    elseif option == oidGlobalContainersToggle
        SetInfoText("$SCIE_InfoGlobalToggle")
    elseif option == oidGlobalCountText
        SetInfoText("$SCIE_InfoGlobalCount")
    elseif option == oidINIContainersToggle
        SetInfoText("$SCIE_InfoINIToggle")
    elseif option == oidFollowerInventoryToggle
        SetInfoText("$SCIE_InfoFollowerToggle")
    elseif option == oidAllowUnsafeToggle
        SetInfoText("$SCIE_InfoAllowUnsafe")
    elseif option == oidScanButton
        SetInfoText("$SCIE_InfoScanCell")
    elseif option == oidSnapshotButton
        SetInfoText("$SCIE_InfoSnapshot")
    elseif option == oidModEnabledToggle
        SetInfoText("$SCIE_InfoModEnabled")
    elseif option == oidAutoGrantPowersToggle
        SetInfoText("$SCIE_InfoAutoGrant")
    elseif option == oidGrantPowersButton
        SetInfoText("$SCIE_InfoGrantPowers")
    elseif option == oidRevokePowersButton
        SetInfoText("$SCIE_InfoRevokePowers")
    elseif option == oidLogLevelMenu
        SetInfoText("$SCIE_InfoLogLevel")
    elseif option == oidClearCellButton
        SetInfoText("$SCIE_InfoClearCell")
    elseif option == oidUninstallButton
        SetInfoText("$SCIE_InfoUninstall")

    ; Containers page nav buttons
    elseif option == oidContainersPrevButton
        SetInfoText("$SCIE_InfoPrevPage")
    elseif option == oidContainersNextButton
        SetInfoText("$SCIE_InfoNextPage")

    ; Filtering page options
    elseif option == oidStationMenu
        SetInfoText("$SCIE_InfoStationSelect")
    elseif option == oidSelectAllButton
        SetInfoText("$SCIE_InfoSelectAll")
    elseif option == oidSelectNoneButton
        SetInfoText("$SCIE_InfoSelectNone")
    elseif option == oidDefaultsButton
        SetInfoText("$SCIE_InfoResetDefaults")
    else
        ; Check if it's a location clear button
        if oidLocationClearButtons
            int ci = 0
            while ci < oidLocationClearButtons.Length
                if option == oidLocationClearButtons[ci] && oidLocationClearButtons[ci] != 0
                    SetInfoText("$SCIE_InfoLocationClear")
                    return
                endif
                ci += 1
            endwhile
        endif

        ; Check if it's a container option
        if oidContainerOptions
            int ci2 = 0
            while ci2 < oidContainerOptions.Length
                if option == oidContainerOptions[ci2] && oidContainerOptions[ci2] != 0
                    SetInfoText("$SCIE_InfoContainerClick")
                    return
                endif
                ci2 += 1
            endwhile
        endif

        ; Check if it's one of the filter toggles
        int i = 0
        while i < 12
            if option == oidFilterToggles[i]
                if SCIE_NativeFunctions.IsFilterLocked(selectedStation, i)
                    SetInfoText("$SCIE_InfoFilterLocked")
                else
                    SetInfoText(formTypeInfoTexts[i])
                endif
                return
            endif
            i += 1
        endwhile
    endif
EndEvent

Event OnOptionSelect(int option)
    if option == oidModEnabledToggle
        bool newValue = !SCIE_NativeFunctions.GetModEnabled()
        SCIE_NativeFunctions.SetModEnabled(newValue)
        SetToggleOptionValue(option, newValue)

    elseif option == oidAutoGrantPowersToggle
        bool newValue = !SCIE_NativeFunctions.GetAutoGrantPowers()
        SCIE_NativeFunctions.SetAutoGrantPowers(newValue)
        SetToggleOptionValue(option, newValue)

    elseif option == oidGlobalContainersToggle
        bool newValue = !SCIE_NativeFunctions.GetEnableGlobalContainers()
        SCIE_NativeFunctions.SetEnableGlobalContainers(newValue)
        SetToggleOptionValue(option, newValue)
        ; Refresh page to update warning text visibility
        ForcePageReset()

    elseif option == oidINIContainersToggle
        bool newValue = !SCIE_NativeFunctions.GetEnableINIContainers()
        SCIE_NativeFunctions.SetEnableINIContainers(newValue)
        SetToggleOptionValue(option, newValue)

    elseif option == oidFollowerInventoryToggle
        bool newValue = !SCIE_NativeFunctions.GetEnableFollowerInventory()
        SCIE_NativeFunctions.SetEnableFollowerInventory(newValue)
        SetToggleOptionValue(option, newValue)

    elseif option == oidAllowUnsafeToggle
        bool newValue = !SCIE_NativeFunctions.GetAllowUnsafeContainers()
        SCIE_NativeFunctions.SetAllowUnsafeContainers(newValue)
        SetToggleOptionValue(option, newValue)

    elseif option == oidGrantPowersButton
        int granted = SCIE_NativeFunctions.GrantPowers()
        if granted > 0
            ShowMessage("$SCIE_FmtGranted{" + granted + "}", false)
        elseif granted < 0
            ShowMessage("$SCIE_MsgPowerFormsNotFound", false)
        else
            ShowMessage("$SCIE_MsgAlreadyHavePowers", false)
        endif

    elseif option == oidRevokePowersButton
        int revoked = SCIE_NativeFunctions.RevokePowers()
        if revoked > 0
            ShowMessage("$SCIE_FmtRevoked{" + revoked + "}", false)
        elseif revoked < 0
            ShowMessage("$SCIE_MsgPowerFormsNotFound", false)
        else
            ShowMessage("$SCIE_MsgNoPowersToRevoke", false)
        endif

    elseif option == oidScanButton
        SCIE_NativeFunctions.DumpContainers()
        ShowMessage("$SCIE_MsgScanComplete", false)

    elseif option == oidSnapshotButton
        string filepath = SCIE_NativeFunctions.SnapshotToggles()
        if filepath != ""
            ShowMessage("$SCIE_FmtSnapshotSaved{" + filepath + "}", false)
        else
            ShowMessage("$SCIE_MsgNoContainersToSnapshot", false)
        endif

    elseif option == oidClearCellButton
        bool confirm = ShowMessage("$SCIE_MsgConfirmClear", true)
        if confirm
            int cleared = SCIE_NativeFunctions.ClearMarkedInCell()
            ShowMessage("$SCIE_FmtCleared{" + cleared + "}", false)
            ; Refresh tracked count display
            ForcePageReset()
        endif

    elseif option == oidUninstallButton
        bool confirm = ShowMessage("$SCIE_MsgConfirmUninstall", true)
        if confirm
            SCIE_NativeFunctions.PrepareForUninstall()
            ShowMessage("$SCIE_MsgUninstallComplete", false)
        endif

    ; Containers page - navigation buttons
    elseif option == oidContainersPrevButton
        if containersPageNum > 0
            containersPageNum -= 1
            ForcePageReset()
        endif

    elseif option == oidContainersNextButton
        containersPageNum += 1
        ForcePageReset()

    ; Filtering page - bulk action buttons
    elseif option == oidSelectAllButton
        SCIE_NativeFunctions.SetAllFilters(selectedStation, true)
        ForcePageReset()

    elseif option == oidSelectNoneButton
        SCIE_NativeFunctions.SetAllFilters(selectedStation, false)
        ForcePageReset()

    elseif option == oidDefaultsButton
        SCIE_NativeFunctions.ResetFilteringToDefaults(selectedStation)
        ForcePageReset()

    else
        ; Check if it's a location clear button
        if HandleLocationClearSelect(option)
            return
        endif

        ; Check if it's a container option (click to cycle state)
        if HandleContainerOptionSelect(option)
            return
        endif

        ; Check if it's one of the filter toggles
        if HandleFilterToggleSelect(option)
            return
        endif

        ; Check if it's one of the SLID network toggles
        if HandleSLIDNetworkToggleSelect(option)
            return
        endif

        ; Check if it's one of the SLID missing network Remove buttons
        if HandleSLIDMissingRemoveSelect(option)
            return
        endif
    endif
EndEvent

Event OnOptionSliderOpen(int option)
    if option == oidDistanceSlider
        float currentValue = SCIE_NativeFunctions.GetMaxDistance()

        ; Dynamically expand range if current value falls outside default bounds
        float minVal = DISTANCE_MIN
        float maxVal = DISTANCE_MAX
        if currentValue < minVal
            minVal = currentValue
        endif
        if currentValue > maxVal
            maxVal = currentValue
        endif

        SetSliderDialogStartValue(currentValue)
        SetSliderDialogDefaultValue(DISTANCE_DEFAULT)
        SetSliderDialogRange(minVal, maxVal)
        SetSliderDialogInterval(DISTANCE_STEP)
    endif
EndEvent

Event OnOptionSliderAccept(int option, float value)
    if option == oidDistanceSlider
        SCIE_NativeFunctions.SetMaxDistance(value)
        SetSliderOptionValue(option, value, "$SCIE_FmtUnits")
        ; Refresh tracked count since distance changed
        ForcePageReset()
    endif
EndEvent

Event OnOptionMenuOpen(int option)
    if option == oidStationMenu
        SetMenuDialogOptions(stationNames)
        SetMenuDialogStartIndex(selectedStation)
        SetMenuDialogDefaultIndex(0)
    elseif option == oidLogLevelMenu
        SetMenuDialogOptions(logLevelNames)
        SetMenuDialogStartIndex(SCIE_NativeFunctions.GetLogLevel())
        SetMenuDialogDefaultIndex(0)  ; Default to Info
    endif
EndEvent

Event OnOptionMenuAccept(int option, int index)
    if option == oidStationMenu
        selectedStation = index
        ; Refresh page to show settings for newly selected station
        ForcePageReset()
    elseif option == oidLogLevelMenu
        SCIE_NativeFunctions.SetLogLevel(index)
        SetMenuOptionValue(option, logLevelNames[index])
    endif
EndEvent

Event OnOptionDefault(int option)
    if option == oidDistanceSlider
        SCIE_NativeFunctions.SetMaxDistance(DISTANCE_DEFAULT)
        SetSliderOptionValue(option, DISTANCE_DEFAULT, "{0} units")
        ForcePageReset()

    elseif option == oidModEnabledToggle
        SCIE_NativeFunctions.SetModEnabled(true)
        SetToggleOptionValue(option, true)

    elseif option == oidAutoGrantPowersToggle
        SCIE_NativeFunctions.SetAutoGrantPowers(true)
        SetToggleOptionValue(option, true)

    elseif option == oidLogLevelMenu
        SCIE_NativeFunctions.SetLogLevel(0)  ; Default to Info
        SetMenuOptionValue(option, logLevelNames[0])

    elseif option == oidGlobalContainersToggle
        SCIE_NativeFunctions.SetEnableGlobalContainers(true)
        SetToggleOptionValue(option, true)
        ForcePageReset()

    elseif option == oidINIContainersToggle
        SCIE_NativeFunctions.SetEnableINIContainers(true)
        SetToggleOptionValue(option, true)

    elseif option == oidFollowerInventoryToggle
        SCIE_NativeFunctions.SetEnableFollowerInventory(true)
        SetToggleOptionValue(option, true)

    elseif option == oidAllowUnsafeToggle
        SCIE_NativeFunctions.SetAllowUnsafeContainers(false)
        SetToggleOptionValue(option, false)
    endif
EndEvent

; =============================================================================
; Helper Functions (for MCM option handling)
; =============================================================================

; Handle click on a location clear button in the Containers page
; Returns true if the option was handled
bool Function HandleLocationClearSelect(int option)
    if !oidLocationClearButtons
        return false
    endif

    int ci = 0
    while ci < oidLocationClearButtons.Length
        if option == oidLocationClearButtons[ci] && oidLocationClearButtons[ci] != 0
            string locName = locationClearNames[ci]
            bool confirm = ShowMessage("$SCIE_FmtConfirmRemove{" + locName + "}", true)
            if confirm
                int removed = SCIE_NativeFunctions.RemovePlayerContainersByLocation(locName)
                ShowMessage("$SCIE_FmtRemoved{" + removed + "}_From{" + locName + "}", false)
                ForcePageReset()
            endif
            return true
        endif
        ci += 1
    endwhile
    return false
EndFunction

; Handle click on a container option in the Containers page
; Returns true if the option was handled
bool Function HandleContainerOptionSelect(int option)
    if !oidContainerOptions
        return false
    endif

    int ci = 0
    while ci < oidContainerOptions.Length
        if option == oidContainerOptions[ci] && oidContainerOptions[ci] != 0
            int globalIdx = containerGlobalIndices[ci]

            ; Fetch current state
            int[] states = SCIE_NativeFunctions.GetPlayerContainerStates(containersPageNum, CONTAINERS_PAGE_SIZE)
            int localIdx = globalIdx - (containersPageNum * CONTAINERS_PAGE_SIZE)
            int curState = states[localIdx]

            ; Cycle: OFF(0) -> LOCAL(1) -> GLOBAL(2) -> OFF(0)
            int newState = 0
            if curState == 0
                newState = 1
            elseif curState == 1
                newState = 2
            else
                newState = 0
            endif

            SCIE_NativeFunctions.SetPlayerContainerState(globalIdx, newState)
            ForcePageReset()
            return true
        endif
        ci += 1
    endwhile
    return false
EndFunction

; Handle click on a filter toggle in the Filtering page
; Returns true if the option was handled
bool Function HandleFilterToggleSelect(int option)
    int i = 0
    while i < 12
        if option == oidFilterToggles[i]
            bool newValue = !SCIE_NativeFunctions.GetFilterSetting(selectedStation, i)
            SCIE_NativeFunctions.SetFilterSetting(selectedStation, i, newValue)
            SetToggleOptionValue(option, newValue)
            return true
        endif
        i += 1
    endwhile
    return false
EndFunction

; Handle click on a SLID network toggle in the Compatibility page
; Returns true if the option was handled
bool Function HandleSLIDNetworkToggleSelect(int option)
    int i = 0
    while i < slidNetworkCount
        if option == oidSLIDNetworkToggles[i]
            string networkName = slidNetworkNames[i]
            bool newValue = !SCIE_NativeFunctions.IsSLIDNetworkEnabled(networkName)
            SCIE_NativeFunctions.SetSLIDNetworkEnabled(networkName, newValue)
            SetToggleOptionValue(option, newValue)
            return true
        endif
        i += 1
    endwhile
    return false
EndFunction

; Handle click on a SLID missing network Remove button in the Compatibility page
; Returns true if the option was handled
bool Function HandleSLIDMissingRemoveSelect(int option)
    int i = 0
    while i < slidMissingCount
        if option == oidSLIDMissingRemove[i]
            string networkName = slidMissingNames[i]
            SCIE_NativeFunctions.RemoveSLIDNetwork(networkName)
            ; Force page refresh to update the list
            ForcePageReset()
            return true
        endif
        i += 1
    endwhile
    return false
EndFunction
