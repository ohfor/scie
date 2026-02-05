#pragma once

#include <mutex>
#include <optional>

namespace Services {
    /// Station type groups for filtering configuration
    /// Maps BenchType values to logical station groups
    enum class StationType : std::uint32_t {
        Crafting = 0,   // BenchType 1 (kCreateObject): forge, smelter, tanning, cooking, staff enchanter
        Tempering,      // BenchType 2+7 (kSmithingWeapon, kSmithingArmor): grindstone, workbench
        Enchanting,     // BenchType 3 (kEnchanting): arcane enchanter
        Alchemy,        // BenchType 5 (kAlchemy): alchemy lab
        COUNT,
        Unknown         // Unrecognized station - no filtering applied (allow all)
    };

    /// Form types that can be filtered for container pulls
    /// Order matches INI key order and MCM display order
    enum class FilterableFormType : std::uint32_t {
        Weapon = 0,     // WEAP
        Armor,          // ARMO
        Misc,           // MISC - ingots, leather, gems, ores
        Ingredient,     // INGR - alchemy ingredients
        AlchemyItem,    // ALCH - potions, food, drinks
        SoulGem,        // SLGM
        Ammo,           // AMMO - arrows, bolts
        Book,           // BOOK
        Scroll,         // SCRL
        Light,          // LIGH - torches
        Key,            // KEYM
        Apparatus,      // APPA - legacy alchemy tools
        COUNT
    };
    /// Manages main INI configuration settings for SCIE.
    /// Note: Container INI files are handled by ContainerRegistry, not here.
    class INISettings {
    public:
        static INISettings* GetSingleton();

        /// Load settings from main INI file
        void Load();

        /// Save current settings to main INI file
        void Save();

        // General settings - Getters
        float GetMaxContainerDistance() const { return m_maxContainerDistance; }
        bool GetConsumeFromPlayerFirst() const { return m_consumeFromPlayerFirst; }
        bool GetEnabled() const { return m_enabled; }
        bool GetAddPowersToPlayer() const { return m_addPowersToPlayer; }
        bool GetEnableGlobalContainers() const { return m_enableGlobalContainers; }
        bool GetEnableINIContainers() const { return m_enableINIContainers; }
        bool GetEnableFollowerInventory() const { return m_enableFollowerInventory; }
        bool GetAllowUnsafeContainers() const { return m_allowUnsafeContainers; }

        // General settings - Setters (called from MCM via Papyrus)
        void SetMaxContainerDistance(float a_distance);
        void SetConsumeFromPlayerFirst(bool a_value);
        void SetEnabled(bool a_value);
        void SetAddPowersToPlayer(bool a_value);
        void SetEnableGlobalContainers(bool a_value);
        void SetEnableINIContainers(bool a_value);
        void SetEnableFollowerInventory(bool a_value);
        void SetAllowUnsafeContainers(bool a_value);

        // Debug settings - Getters
        bool GetDebugLogging() const { return m_debugLogging; }

        // Debug settings - Setters (called from MCM via Papyrus)
        void SetDebugLogging(bool a_value);

        // Filtering settings - check if a form type should be pulled from containers
        // This is the main runtime check called by InventoryHooks
        bool ShouldPullFormType(StationType a_station, RE::FormType a_formType) const;

        // Check if a filter combination is locked (cannot be enabled by the user).
        // WEAP/ARMO at Tempering/Enchanting are locked because in-place item modification
        // on container items causes crashes, duplication, or data corruption.
        static bool IsFilterLocked(StationType a_station, FilterableFormType a_type);

        // Filtering settings - individual get/set for MCM
        bool GetFilterSetting(StationType a_station, FilterableFormType a_type) const;
        void SetFilterSetting(StationType a_station, FilterableFormType a_type, bool a_enabled);

        // Filtering settings - bulk operations for MCM buttons
        void SetAllFilters(StationType a_station, bool a_enabled);
        void ResetFilteringToDefaults(StationType a_station);
        void ResetAllFilteringToDefaults();

        /// Get human-readable name for a station type (for logging/debug)
        static const char* GetStationKeyPrefix(StationType a_station);

        /// Apply current debug logging setting to spdlog level
        void ApplyLogLevel();

        /// Get the main INI file path
        static std::filesystem::path GetINIPath() {
            return "Data/SKSE/Plugins/CraftingInventoryExtender.ini";
        }

    private:
        INISettings() = default;
        ~INISettings() = default;
        INISettings(const INISettings&) = delete;
        INISettings& operator=(const INISettings&) = delete;

        /// Parse the main INI file
        void ParseMainINI(const std::filesystem::path& a_path);

        /// Helper to parse a boolean value from string
        static bool ParseBool(const std::string& a_value, bool a_default);

        /// Helper to parse a float value from string
        static float ParseFloat(const std::string& a_value, float a_default);

        // Thread safety for settings access
        mutable std::mutex m_settingsMutex;

        // General settings (defaults match INI defaults)
        float m_maxContainerDistance = 3000.0f;
        bool m_consumeFromPlayerFirst = true;  // Not yet implemented
        bool m_enabled = true;
        bool m_addPowersToPlayer = true;
        bool m_enableGlobalContainers = true;  // Can disable for LOTD compatibility
        bool m_enableINIContainers = true;      // Can disable all INI-configured containers
        bool m_enableFollowerInventory = true;  // Auto-include followers/spouses at crafting
        bool m_allowUnsafeContainers = false;   // Allow toggling containers with respawn flag

        // Debug settings
        bool m_debugLogging = false;

        // Filtering settings - 4 station types x 12 form types = 48 booleans
        // Indexed as m_filterSettings[stationType][formType]
        // Defaults are set in ResetAllFilteringToDefaults()
        bool m_filterSettings[static_cast<size_t>(StationType::COUNT)]
                             [static_cast<size_t>(FilterableFormType::COUNT)];

        /// Convert RE::FormType to our FilterableFormType enum
        /// Returns std::nullopt if the form type is not filterable
        static std::optional<FilterableFormType> FormTypeToFilterable(RE::FormType a_type);

        /// Get INI key suffix for a filterable form type (e.g., "WEAP", "ARMO")
        static const char* GetFormTypeKeySuffix(FilterableFormType a_type);
    };
}
