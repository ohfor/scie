#include "Services/INISettings.h"

namespace Services {
    INISettings* INISettings::GetSingleton() {
        static INISettings singleton;
        return &singleton;
    }

    void INISettings::Load() {
        logger::info("Loading INI settings...");

        // Initialize filter defaults before loading (so INI can override)
        ResetAllFilteringToDefaults();

        auto iniPath = GetINIPath();

        if (std::filesystem::exists(iniPath)) {
            ParseMainINI(iniPath);
            logger::info("Loaded settings from {}", iniPath.string());
        } else {
            logger::info("INI file not found at {}, using defaults", iniPath.string());
        }

        logger::info("Settings: Enabled={}, MaxDistance={:.0f}, PlayerFirst={}, AddPowers={}, GlobalContainers={}, INIContainers={}, FollowerInv={}, AllowUnsafe={}, DebugLog={}",
            m_enabled, m_maxContainerDistance, m_consumeFromPlayerFirst, m_addPowersToPlayer, m_enableGlobalContainers, m_enableINIContainers, m_enableFollowerInventory, m_allowUnsafeContainers, m_debugLogging);

        // Apply debug logging setting
        ApplyLogLevel();

        // Note: Container INI files are loaded by ContainerRegistry, not here
    }

    void INISettings::ParseMainINI(const std::filesystem::path& a_path) {
        std::ifstream file(a_path);
        if (!file.is_open()) {
            logger::warn("Failed to open INI file: {}", a_path.string());
            return;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(file, line)) {
            // Trim leading whitespace
            auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            // Trim trailing whitespace and newlines
            auto end = line.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }

            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            // Check for section header
            if (line[0] == '[') {
                auto closePos = line.find(']');
                if (closePos != std::string::npos) {
                    currentSection = line.substr(1, closePos - 1);
                    // Convert to lowercase for case-insensitive matching
                    std::transform(currentSection.begin(), currentSection.end(),
                                   currentSection.begin(), ::tolower);
                }
                continue;
            }

            // Parse key=value
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            // Trim key and value
            auto trimStr = [](std::string& s) {
                auto st = s.find_first_not_of(" \t");
                auto en = s.find_last_not_of(" \t");
                if (st != std::string::npos && en != std::string::npos) {
                    s = s.substr(st, en - st + 1);
                } else {
                    s.clear();
                }
            };
            trimStr(key);
            trimStr(value);

            // Strip inline comments from value
            auto commentPos = value.find(';');
            if (commentPos != std::string::npos) {
                value = value.substr(0, commentPos);
                trimStr(value);
            }

            // Convert key to lowercase for case-insensitive matching
            std::string keyLower = key;
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

            // Apply settings based on section and key
            if (currentSection == "general") {
                if (keyLower == "benabled") {
                    m_enabled = ParseBool(value, m_enabled);
                } else if (keyLower == "fmaxcontainerdistance") {
                    m_maxContainerDistance = ParseFloat(value, m_maxContainerDistance);
                } else if (keyLower == "bconsumefromplayerfirst") {
                    m_consumeFromPlayerFirst = ParseBool(value, m_consumeFromPlayerFirst);
                } else if (keyLower == "baddpowerstoplayer") {
                    m_addPowersToPlayer = ParseBool(value, m_addPowersToPlayer);
                } else if (keyLower == "benableglobalcontainers") {
                    m_enableGlobalContainers = ParseBool(value, m_enableGlobalContainers);
                } else if (keyLower == "benableinicontainers") {
                    m_enableINIContainers = ParseBool(value, m_enableINIContainers);
                } else if (keyLower == "benablefollowerinventory") {
                    m_enableFollowerInventory = ParseBool(value, m_enableFollowerInventory);
                } else if (keyLower == "ballowunsafecontainers") {
                    m_allowUnsafeContainers = ParseBool(value, m_allowUnsafeContainers);
                }
            } else if (currentSection == "debug") {
                if (keyLower == "bdebuglogging") {
                    m_debugLogging = ParseBool(value, m_debugLogging);
                }
            } else if (currentSection == "filtering") {
                // Parse keys like "bCrafting_WEAP", "bTempering_MISC", etc.
                // Format: b<StationType>_<FormType>=true/false
                if (keyLower.size() > 1 && keyLower[0] == 'b') {
                    auto underscorePos = keyLower.find('_');
                    if (underscorePos != std::string::npos && underscorePos > 1) {
                        std::string stationPart = keyLower.substr(1, underscorePos - 1);
                        std::string formPart = keyLower.substr(underscorePos + 1);

                        // Convert to uppercase for form type matching
                        std::transform(formPart.begin(), formPart.end(), formPart.begin(), ::toupper);

                        // Determine station type
                        StationType station = StationType::COUNT;
                        if (stationPart == "crafting") station = StationType::Crafting;
                        else if (stationPart == "tempering") station = StationType::Tempering;
                        else if (stationPart == "enchanting") station = StationType::Enchanting;
                        else if (stationPart == "alchemy") station = StationType::Alchemy;

                        // Determine form type
                        FilterableFormType formType = FilterableFormType::COUNT;
                        if (formPart == "WEAP") formType = FilterableFormType::Weapon;
                        else if (formPart == "ARMO") formType = FilterableFormType::Armor;
                        else if (formPart == "MISC") formType = FilterableFormType::Misc;
                        else if (formPart == "INGR") formType = FilterableFormType::Ingredient;
                        else if (formPart == "ALCH") formType = FilterableFormType::AlchemyItem;
                        else if (formPart == "SLGM") formType = FilterableFormType::SoulGem;
                        else if (formPart == "AMMO") formType = FilterableFormType::Ammo;
                        else if (formPart == "BOOK") formType = FilterableFormType::Book;
                        else if (formPart == "SCRL") formType = FilterableFormType::Scroll;
                        else if (formPart == "LIGH") formType = FilterableFormType::Light;
                        else if (formPart == "KEYM") formType = FilterableFormType::Key;
                        else if (formPart == "APPA") formType = FilterableFormType::Apparatus;

                        if (station != StationType::COUNT && formType != FilterableFormType::COUNT) {
                            auto stationIdx = static_cast<size_t>(station);
                            auto formIdx = static_cast<size_t>(formType);
                            bool parsed = ParseBool(value, m_filterSettings[stationIdx][formIdx]);
                            if (parsed && IsFilterLocked(station, formType)) {
                                logger::info("INI has locked filter {}_{} set to true — forcing to false",
                                    GetStationKeyPrefix(station), GetFormTypeKeySuffix(formType));
                                parsed = false;
                            }
                            m_filterSettings[stationIdx][formIdx] = parsed;
                        }
                    }
                }
            }
        }
    }

    bool INISettings::ParseBool(const std::string& a_value, bool a_default) {
        if (a_value.empty()) return a_default;

        std::string lower = a_value;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "true" || lower == "1" || lower == "yes") {
            return true;
        } else if (lower == "false" || lower == "0" || lower == "no") {
            return false;
        }

        return a_default;
    }

    float INISettings::ParseFloat(const std::string& a_value, float a_default) {
        if (a_value.empty()) return a_default;

        try {
            return std::stof(a_value);
        } catch (const std::exception&) {
            return a_default;
        }
    }

    void INISettings::Save() {
        std::lock_guard lock(m_settingsMutex);

        auto iniPath = GetINIPath();

        std::ofstream file(iniPath);
        if (!file.is_open()) {
            logger::error("Failed to open INI file for writing: {}", iniPath.string());
            return;
        }

        file << "; Crafting Inventory Extender - Main Configuration\n";
        file << "; ================================================\n";
        file << "\n";
        file << "[General]\n";
        file << "; Enable/disable the plugin entirely\n";
        file << "bEnabled=" << (m_enabled ? "true" : "false") << "\n";
        file << "\n";
        file << "; Automatically add SCIE powers to player on game load\n";
        file << "bAddPowersToPlayer=" << (m_addPowersToPlayer ? "true" : "false") << "\n";
        file << "\n";
        file << "; Maximum distance (in game units) to search for containers\n";
        file << "; 768 = small room, 1500 = large room, 3000 = whole building\n";
        file << "fMaxContainerDistance=" << std::fixed << std::setprecision(1) << m_maxContainerDistance << "\n";
        file << "\n";
        file << "; Consumption priority when crafting (not yet implemented)\n";
        file << "; true = consume from player inventory first, then containers\n";
        file << "; false = consume from containers first, then player inventory\n";
        file << "bConsumeFromPlayerFirst=" << (m_consumeFromPlayerFirst ? "true" : "false") << "\n";
        file << "\n";
        file << "; Enable global containers (LOTD Safehouse, General Stores, etc.)\n";
        file << "; Disable if using LOTD's CraftLoot feature to avoid conflicts\n";
        file << "bEnableGlobalContainers=" << (m_enableGlobalContainers ? "true" : "false") << "\n";
        file << "\n";
        file << "; Enable containers pre-configured in INI files (player home presets, etc.)\n";
        file << "; Disable if you prefer to mark all containers yourself with the toggle power\n";
        file << "bEnableINIContainers=" << (m_enableINIContainers ? "true" : "false") << "\n";
        file << "\n";
        file << "; Include nearby follower/spouse inventories at crafting stations\n";
        file << "; Safe items only: weapons, armor, ammo, and potions are never taken\n";
        file << "bEnableFollowerInventory=" << (m_enableFollowerInventory ? "true" : "false") << "\n";
        file << "\n";
        file << "; Allow toggling containers that have the respawn flag (unsafe storage)\n";
        file << "; When false, the toggle power will refuse to mark respawning containers\n";
        file << "bAllowUnsafeContainers=" << (m_allowUnsafeContainers ? "true" : "false") << "\n";
        file << "\n";
        file << "[Debug]\n";
        file << "; Enable verbose logging for troubleshooting\n";
        file << "bDebugLogging=" << (m_debugLogging ? "true" : "false") << "\n";
        file << "\n";

        // Write filtering settings
        file << "[Filtering]\n";
        file << "; Configure which form types to pull from containers per station type\n";
        file << "; WEAP=Weapons, ARMO=Armor, MISC=Misc (ingots, leather), INGR=Ingredients\n";
        file << "; ALCH=Potions/Food, SLGM=Soul Gems, AMMO=Ammunition, BOOK=Books\n";
        file << "; SCRL=Scrolls, LIGH=Lights, KEYM=Keys, APPA=Apparatus\n";
        file << "\n";

        // Write all 48 settings organized by station
        for (size_t s = 0; s < static_cast<size_t>(StationType::COUNT); ++s) {
            auto station = static_cast<StationType>(s);
            file << "; " << GetStationKeyPrefix(station) << " station\n";

            for (size_t f = 0; f < static_cast<size_t>(FilterableFormType::COUNT); ++f) {
                auto formType = static_cast<FilterableFormType>(f);
                file << "b" << GetStationKeyPrefix(station) << "_" << GetFormTypeKeySuffix(formType)
                     << "=" << (m_filterSettings[s][f] ? "true" : "false") << "\n";
            }
            file << "\n";
        }

        file.close();
        logger::info("Saved settings to {}", iniPath.string());
    }

    // Setters - update value and optionally save to INI
    void INISettings::SetMaxContainerDistance(float a_distance) {
        std::lock_guard lock(m_settingsMutex);
        m_maxContainerDistance = std::clamp(a_distance, 100.0f, 10000.0f);
        logger::info("MaxContainerDistance set to {:.0f}", m_maxContainerDistance);
    }

    void INISettings::SetConsumeFromPlayerFirst(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_consumeFromPlayerFirst = a_value;
        logger::info("ConsumeFromPlayerFirst set to {}", a_value);
    }

    void INISettings::SetEnabled(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_enabled = a_value;
        logger::info("Enabled set to {}", a_value);
    }

    void INISettings::SetAddPowersToPlayer(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_addPowersToPlayer = a_value;
        logger::info("AddPowersToPlayer set to {}", a_value);
    }

    void INISettings::SetEnableGlobalContainers(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_enableGlobalContainers = a_value;
        logger::info("EnableGlobalContainers set to {}", a_value);
    }

    void INISettings::SetEnableINIContainers(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_enableINIContainers = a_value;
        logger::info("EnableINIContainers set to {}", a_value);
    }

    void INISettings::SetEnableFollowerInventory(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_enableFollowerInventory = a_value;
        logger::info("EnableFollowerInventory set to {}", a_value);
    }

    void INISettings::SetAllowUnsafeContainers(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_allowUnsafeContainers = a_value;
        logger::info("AllowUnsafeContainers set to {}", a_value);
    }

    void INISettings::SetDebugLogging(bool a_value) {
        std::lock_guard lock(m_settingsMutex);
        m_debugLogging = a_value;
        logger::info("DebugLogging set to {}", a_value);
        ApplyLogLevel();
    }

    void INISettings::ApplyLogLevel() {
        auto level = m_debugLogging ? spdlog::level::debug : spdlog::level::info;
        spdlog::set_level(level);
        spdlog::default_logger()->flush_on(level);  // Also flush at this level
        logger::info("Log level set to {}", m_debugLogging ? "debug" : "info");
    }

    // ========================================================================
    // Filtering Settings Implementation
    // ========================================================================

    std::optional<FilterableFormType> INISettings::FormTypeToFilterable(RE::FormType a_type) {
        switch (a_type) {
            case RE::FormType::Weapon:      return FilterableFormType::Weapon;
            case RE::FormType::Armor:       return FilterableFormType::Armor;
            case RE::FormType::Misc:        return FilterableFormType::Misc;
            case RE::FormType::Ingredient:  return FilterableFormType::Ingredient;
            case RE::FormType::AlchemyItem: return FilterableFormType::AlchemyItem;
            case RE::FormType::SoulGem:     return FilterableFormType::SoulGem;
            case RE::FormType::Ammo:        return FilterableFormType::Ammo;
            case RE::FormType::Book:        return FilterableFormType::Book;
            case RE::FormType::Scroll:      return FilterableFormType::Scroll;
            case RE::FormType::Light:       return FilterableFormType::Light;
            case RE::FormType::KeyMaster:   return FilterableFormType::Key;
            case RE::FormType::Apparatus:   return FilterableFormType::Apparatus;
            default:                        return std::nullopt;
        }
    }

    const char* INISettings::GetFormTypeKeySuffix(FilterableFormType a_type) {
        switch (a_type) {
            case FilterableFormType::Weapon:      return "WEAP";
            case FilterableFormType::Armor:       return "ARMO";
            case FilterableFormType::Misc:        return "MISC";
            case FilterableFormType::Ingredient:  return "INGR";
            case FilterableFormType::AlchemyItem: return "ALCH";
            case FilterableFormType::SoulGem:     return "SLGM";
            case FilterableFormType::Ammo:        return "AMMO";
            case FilterableFormType::Book:        return "BOOK";
            case FilterableFormType::Scroll:      return "SCRL";
            case FilterableFormType::Light:       return "LIGH";
            case FilterableFormType::Key:         return "KEYM";
            case FilterableFormType::Apparatus:   return "APPA";
            default:                              return "UNKN";
        }
    }

    const char* INISettings::GetStationKeyPrefix(StationType a_station) {
        switch (a_station) {
            case StationType::Crafting:   return "Crafting";
            case StationType::Tempering:  return "Tempering";
            case StationType::Enchanting: return "Enchanting";
            case StationType::Alchemy:    return "Alchemy";
            case StationType::Unknown:    return "Unknown";
            default:                      return "Invalid";
        }
    }

    bool INISettings::ShouldPullFormType(StationType a_station, RE::FormType a_formType) const {
        // Unknown station type = no filtering, allow everything
        // This is a safety fallback for unrecognized crafting stations
        if (a_station == StationType::Unknown) {
            return true;
        }

        auto filterable = FormTypeToFilterable(a_formType);
        if (!filterable.has_value()) {
            // Form type not in our filter list - don't pull by default
            // This covers things like ACTI, FLOR, etc. that shouldn't be in crafting anyway
            return false;
        }

        // Hard block: locked filters can never be enabled, regardless of stored state
        if (IsFilterLocked(a_station, filterable.value())) {
            return false;
        }

        std::lock_guard lock(m_settingsMutex);
        auto stationIdx = static_cast<size_t>(a_station);
        auto formIdx = static_cast<size_t>(filterable.value());

        if (stationIdx >= static_cast<size_t>(StationType::COUNT) ||
            formIdx >= static_cast<size_t>(FilterableFormType::COUNT)) {
            return false;
        }

        return m_filterSettings[stationIdx][formIdx];
    }

    bool INISettings::IsFilterLocked(StationType a_station, FilterableFormType a_type) {
        // WEAP/ARMO at Tempering/Enchanting are locked off.
        // These stations modify items in-place via InventoryChanges, which requires the item
        // to physically exist in the player's inventory. Container items cause crashes,
        // duplication, or data corruption because the game writes to the wrong InventoryChanges.
        if (a_type == FilterableFormType::Weapon || a_type == FilterableFormType::Armor) {
            if (a_station == StationType::Tempering || a_station == StationType::Enchanting) {
                return true;
            }
        }
        return false;
    }

    bool INISettings::GetFilterSetting(StationType a_station, FilterableFormType a_type) const {
        std::lock_guard lock(m_settingsMutex);
        auto stationIdx = static_cast<size_t>(a_station);
        auto formIdx = static_cast<size_t>(a_type);

        if (stationIdx >= static_cast<size_t>(StationType::COUNT) ||
            formIdx >= static_cast<size_t>(FilterableFormType::COUNT)) {
            return false;
        }

        return m_filterSettings[stationIdx][formIdx];
    }

    void INISettings::SetFilterSetting(StationType a_station, FilterableFormType a_type, bool a_enabled) {
        if (a_enabled && IsFilterLocked(a_station, a_type)) {
            logger::warn("Filter {}_{} is locked off — WEAP/ARMO cannot be enabled at Tempering/Enchanting stations",
                GetStationKeyPrefix(a_station), GetFormTypeKeySuffix(a_type));
            return;
        }

        std::lock_guard lock(m_settingsMutex);
        auto stationIdx = static_cast<size_t>(a_station);
        auto formIdx = static_cast<size_t>(a_type);

        if (stationIdx >= static_cast<size_t>(StationType::COUNT) ||
            formIdx >= static_cast<size_t>(FilterableFormType::COUNT)) {
            return;
        }

        m_filterSettings[stationIdx][formIdx] = a_enabled;
        logger::debug("Filter {}_{} set to {}",
            GetStationKeyPrefix(a_station), GetFormTypeKeySuffix(a_type), a_enabled);
    }

    void INISettings::SetAllFilters(StationType a_station, bool a_enabled) {
        std::lock_guard lock(m_settingsMutex);
        auto stationIdx = static_cast<size_t>(a_station);

        if (stationIdx >= static_cast<size_t>(StationType::COUNT)) {
            return;
        }

        for (size_t i = 0; i < static_cast<size_t>(FilterableFormType::COUNT); ++i) {
            auto formType = static_cast<FilterableFormType>(i);
            if (a_enabled && IsFilterLocked(a_station, formType)) {
                m_filterSettings[stationIdx][i] = false;
            } else {
                m_filterSettings[stationIdx][i] = a_enabled;
            }
        }

        logger::info("All filters for {} set to {} (locked filters skipped)", GetStationKeyPrefix(a_station), a_enabled);
    }

    void INISettings::ResetFilteringToDefaults(StationType a_station) {
        std::lock_guard lock(m_settingsMutex);
        auto idx = static_cast<size_t>(a_station);

        if (idx >= static_cast<size_t>(StationType::COUNT)) {
            return;
        }

        // Default: all OFF
        for (size_t i = 0; i < static_cast<size_t>(FilterableFormType::COUNT); ++i) {
            m_filterSettings[idx][i] = false;
        }

        // Per-station defaults from feature plan
        switch (a_station) {
            case StationType::Crafting:
                // Most permissive - forge/cooking/tanning needs variety
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Misc)] = true;        // Ingots, leather
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Ingredient)] = true;  // Cooking
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::AlchemyItem)] = true; // Food for cooking
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::SoulGem)] = true;     // Staff enchanter
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Ammo)] = true;        // Arrow crafting
                break;

            case StationType::Tempering:
                // Only materials - you temper YOUR gear
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Misc)] = true;
                break;

            case StationType::Enchanting:
                // Soul gems + misc (for disenchanting materials maybe)
                // WEAP/ARMO off - prevents duplicates, ensures result goes to player
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Misc)] = true;
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::SoulGem)] = true;
                break;

            case StationType::Alchemy:
                // Ingredients and potions/food only
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::Ingredient)] = true;
                m_filterSettings[idx][static_cast<size_t>(FilterableFormType::AlchemyItem)] = true;
                break;

            default:
                break;
        }

        logger::info("Filter defaults applied for {}", GetStationKeyPrefix(a_station));
    }

    void INISettings::ResetAllFilteringToDefaults() {
        // Don't lock here - ResetFilteringToDefaults will lock
        for (size_t i = 0; i < static_cast<size_t>(StationType::COUNT); ++i) {
            ResetFilteringToDefaults(static_cast<StationType>(i));
        }
        logger::info("All filter defaults applied");
    }

}
