#pragma once

namespace Services {

    /// Container state for player overrides (3-state toggle cycle)
    enum class ContainerState : std::uint8_t {
        kOff = 0,     // Player disabled this container
        kLocal = 1,   // Local container (nearby access only)
        kGlobal = 2   // Global container (accessible from anywhere)
    };

    /// Metadata for an INI-configured container (for MCM display)
    struct INIContainerEntry {
        RE::FormID formID;
        bool isGlobal;       // [GlobalContainers] vs [Containers]
        std::string plugin;  // Source plugin name (e.g., "Skyrim.esm")
    };

    /// Metadata for a player-overridden container
    struct ContainerInfo {
        RE::FormID formID = 0;
        ContainerState state = ContainerState::kOff;
        std::string name;              // Container display name (captured at toggle time)
        std::string location;          // Cell/location name (captured at toggle time, cosaved)
        std::string displayLocation;   // Transient fallback (ESP filename) - NOT cosaved
    };

    /// Central registry for container crafting eligibility.
    /// Combines INI-configured defaults with player overrides (cosave).
    class ContainerRegistry {
    public:
        static ContainerRegistry* GetSingleton();

        /// Initialize the registry - load INI files
        void Initialize();

        /// Check if a container should be used for crafting
        /// Priority: player override > global INI > local INI
        bool ShouldPullFrom(RE::TESObjectREFR* a_container) const;
        bool ShouldPullFrom(RE::FormID a_formID) const;

        /// Cycle a container's state: off -> local -> global -> off
        /// Returns the new state as int (0=off, 1=local, 2=global)
        /// Captures name and location on first toggle (unmarked -> local)
        int ToggleCycleState(RE::FormID a_formID, RE::TESObjectREFR* a_ref = nullptr);

        /// Get the effective state of a container (considers INI + overrides)
        /// Returns: 0=off, 1=local, 2=global
        int GetContainerState(RE::FormID a_formID) const;

        /// Check individual status (legacy compatibility)
        bool IsINIConfigured(RE::FormID a_formID) const;
        bool IsManuallyEnabled(RE::FormID a_formID) const;
        bool IsManuallyDisabled(RE::FormID a_formID) const;

        /// Clear all player overrides (called on revert/new game)
        void ClearPlayerOverrides();

        /// Get counts for debugging/UI
        std::size_t GetINIConfiguredCount() const;
        std::size_t GetManuallyEnabledCount() const;
        std::size_t GetManuallyDisabledCount() const;

        /// Dump all containers in current cell to SKSE log (for INI generation)
        void DumpContainersToLog();

        /// Generate a snapshot INI file from player-marked containers in current cell
        /// Returns the filepath of the generated file, or empty string on failure
        std::string GenerateSnapshot();

        /// Clear player overrides for containers in current cell only
        /// Returns count of overrides cleared
        std::int32_t ClearOverridesInCell(float a_maxDistance = 0.0f);

        /// Get containers in current cell for the Detect Power
        /// Returns containers that would be used for crafting (player-enabled OR INI-configured)
        std::vector<RE::TESObjectREFR*> GetEnabledContainersInCell(float a_maxDistance = 0.0f);

        /// Returns containers that were explicitly disabled by player (overriding INI)
        std::vector<RE::TESObjectREFR*> GetDisabledContainersInCell(float a_maxDistance = 0.0f);

        /// Get containers in current cell that are in global state
        std::vector<RE::TESObjectREFR*> GetGlobalContainersInCell(float a_maxDistance = 0.0f);

        /// Check if a container is configured as a global container (INI or player-promoted)
        bool IsGlobalContainer(RE::FormID a_formID) const;

        /// Check if a container is an INI-configured global container
        bool IsINIGlobalContainer(RE::FormID a_formID) const;

        /// Get all global containers (resolved refs, filtered by player overrides)
        /// These are accessible from anywhere, regardless of player location
        std::vector<RE::TESObjectREFR*> GetGlobalContainers() const;

        /// Get count of global containers configured
        std::size_t GetGlobalContainerCount() const;

        /// Get the player overrides map (for MCM Containers page)
        std::unordered_map<RE::FormID, ContainerInfo> GetPlayerOverrides() const;

        /// Set a specific container's state from MCM (0-indexed into sorted list)
        void SetContainerStateByIndex(int a_index, ContainerState a_newState);

        /// Set a specific container's state by FormID
        void SetContainerState(RE::FormID a_formID, ContainerState a_newState);

        /// Remove all player overrides for a given display location (for MCM per-section clear)
        /// Returns count of removed overrides
        int RemovePlayerContainersByLocation(const std::string& a_location);

        /// Get containers grouped by location, sorted
        /// Returns vector of (location, vector<ContainerInfo>) pairs
        std::vector<std::pair<std::string, std::vector<ContainerInfo>>> GetContainersGroupedByLocation() const;

        /// Get total count of player overrides (for MCM)
        std::size_t GetPlayerOverrideCount() const;

        /// Paginated access to player overrides (sorted by location then name)
        /// Returns parallel arrays of names, locations, states for MCM
        struct PlayerContainerPage {
            std::vector<std::string> names;
            std::vector<std::string> locations;
            std::vector<int> states;
        };
        PlayerContainerPage GetPlayerContainerPage(int a_page, int a_pageSize) const;

        /// INI source display for MCM Containers page
        std::size_t GetINISourceCount() const;
        std::vector<std::string> GetINISourceNames() const;

        struct INISourceData {
            std::vector<std::string> names;     // container display names (with suffixes)
            std::vector<int> states;            // 1=local, 2=global
            std::vector<std::string> plugins;   // source plugin for each container
            std::vector<std::string> locations; // cell/location name for each container
            std::vector<int> reachable;         // 1=reachable now, 0=out of range/unloaded
        };
        INISourceData GetINISourceContainers(const std::string& a_displayName, bool a_includeLocal, bool a_includeGlobal) const;

        /// Check if Legacy of the Dragonborn is installed (for compatibility info)
        static bool IsLOTDInstalled();

        /// Check if General Stores is installed (for compatibility info)
        static bool IsGeneralStoresInstalled();

        /// Check if Linked Crafting Storage is installed (incompatible mod warning)
        static bool IsLinkedCraftingStorageInstalled();

        /// Check if Convenient Horses is installed (for compatibility info)
        static bool IsConvenientHorsesInstalled();

        /// Check if Hip Bag is installed (for compatibility info)
        static bool IsHipBagInstalled();

        /// Check if Nether's Follower Framework is installed (for compatibility info)
        static bool IsNFFInstalled();

        /// Check if Khajiit Will Follow is installed (for compatibility info)
        static bool IsKWFInstalled();

        /// Check if Essential Favorites SKSE plugin is loaded
        static bool IsEssentialFavoritesInstalled();

        /// Check if Favorite Misc Items SKSE plugin is loaded
        static bool IsFavoriteMiscItemsInstalled();

        /// Get count of favorited items excluded from current session
        static std::size_t GetFavoritedItemsExcludedCount();

        // Cosave serialization
        static constexpr std::uint32_t kCosaveID = 'SCIE';
        static constexpr std::uint32_t kCosaveVersion = 2;

        void SaveToCoSave(SKSE::SerializationInterface* a_intfc);
        void LoadFromCoSave(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version);
        void Revert(SKSE::SerializationInterface* a_intfc);

        // Static callbacks for SKSE
        static void OnGameSaved(SKSE::SerializationInterface* a_intfc);
        static void OnGameLoaded(SKSE::SerializationInterface* a_intfc);
        static void OnRevert(SKSE::SerializationInterface* a_intfc);

    private:
        ContainerRegistry() = default;
        ~ContainerRegistry() = default;
        ContainerRegistry(const ContainerRegistry&) = delete;
        ContainerRegistry& operator=(const ContainerRegistry&) = delete;

        /// Load all INI files from config folder
        void LoadINIFiles();

        /// Parse a single INI file
        void ParseINIFile(const std::filesystem::path& a_path);

        /// Parse a container entry line: "Plugin|FormID = true" or "Plugin|EditorID = true"
        /// Returns resolved FormID or nullopt if invalid/plugin not loaded
        std::optional<RE::FormID> ParseContainerEntry(const std::string& a_key, const std::string& a_value);

        /// Resolve FormID from plugin + local ID
        std::optional<RE::FormID> ResolveFormID(const std::string& a_plugin, std::uint32_t a_localFormID);

        /// Resolve EditorID to FormID (cached)
        std::optional<RE::FormID> ResolveEditorID(const std::string& a_plugin, const std::string& a_editorID);

        /// Get the INI baseline state for a container
        /// Returns kLocal if in m_iniConfigured, kGlobal if in m_globalContainers, kOff otherwise
        ContainerState GetINIBaselineState(RE::FormID a_formID) const;

        /// Backfill name/location for overrides with empty metadata
        void BackfillOverrideMetadata();

        /// Get a sorted list of all overrides (for paginated MCM access)
        std::vector<ContainerInfo> GetSortedOverrides() const;

        /// Capture name and location from a container reference
        static std::pair<std::string, std::string> CaptureContainerMetadata(RE::TESObjectREFR* a_ref);

        // INI-configured containers (loaded at startup)
        std::unordered_set<RE::FormID> m_iniConfigured;
        mutable std::mutex m_iniMutex;

        // Global containers - accessible from anywhere (loaded from [GlobalContainers] section)
        std::unordered_set<RE::FormID> m_globalContainers;
        mutable std::mutex m_globalMutex;

        // Player overrides (saved in cosave) - replaces old m_manuallyEnabled/m_manuallyDisabled
        std::unordered_map<RE::FormID, ContainerInfo> m_playerOverrides;
        mutable std::mutex m_overridesMutex;

        // INI source tracking for MCM display (populated during Initialize, read-only after)
        std::map<std::string, std::vector<INIContainerEntry>> m_iniSources;

        // EditorID -> FormID cache (for deferred resolution)
        struct PendingEditorID {
            std::string plugin;
            std::string editorID;
        };
        std::vector<PendingEditorID> m_pendingEditorIDs;
        std::unordered_map<std::string, RE::FormID> m_editorIDCache;
        mutable std::mutex m_editorIDMutex;
    };
}
