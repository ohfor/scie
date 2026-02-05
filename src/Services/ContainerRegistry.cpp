#include "Services/ContainerRegistry.h"
#include "Services/ContainerUtils.h"
#include "Services/INISettings.h"
#include "Services/TranslationService.h"
#include "Hooks/CraftingSession.h"

namespace Services {

    namespace {
        /// Get the plugin name and local FormID for a given form
        std::pair<std::string, std::uint32_t> GetPluginInfo(RE::TESForm* a_form) {
            if (!a_form) return {"", 0};

            auto formID = a_form->GetFormID();
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return {"", formID};

            // Check if it's a light plugin (0xFEXXXYYY)
            if ((formID >> 24) == 0xFE) {
                auto lightIndex = (formID >> 12) & 0xFFF;
                auto localFormID = formID & 0xFFF;

                for (auto* file : dataHandler->files) {
                    if (file && file->IsLight() && file->GetSmallFileCompileIndex() == lightIndex) {
                        return {file->fileName, localFormID};
                    }
                }
            } else {
                auto modIndex = formID >> 24;
                auto localFormID = formID & 0x00FFFFFF;

                for (auto* file : dataHandler->files) {
                    if (file && !file->IsLight() && file->GetCompileIndex() == modIndex) {
                        return {file->fileName, localFormID};
                    }
                }
            }

            return {"Unknown", formID};
        }

        /// Get the ESP filename for a FormID (for display location fallback)
        std::string GetESPFilename(RE::FormID a_formID) {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return "";

            if ((a_formID >> 24) == 0xFE) {
                auto lightIndex = (a_formID >> 12) & 0xFFF;
                for (auto* file : dataHandler->files) {
                    if (file && file->IsLight() && file->GetSmallFileCompileIndex() == lightIndex) {
                        return file->fileName;
                    }
                }
            } else {
                auto modIndex = a_formID >> 24;
                for (auto* file : dataHandler->files) {
                    if (file && !file->IsLight() && file->GetCompileIndex() == modIndex) {
                        return file->fileName;
                    }
                }
            }
            return "";
        }
    }  // anonymous namespace

    ContainerRegistry* ContainerRegistry::GetSingleton() {
        static ContainerRegistry singleton;
        return &singleton;
    }

    void ContainerRegistry::Initialize() {
        logger::info("Initializing ContainerRegistry...");
        LoadINIFiles();
        BackfillOverrideMetadata();
        logger::info("ContainerRegistry initialized: {} local containers, {} global containers, {} player overrides",
            m_iniConfigured.size(), m_globalContainers.size(), m_playerOverrides.size());
    }

    bool ContainerRegistry::ShouldPullFrom(RE::TESObjectREFR* a_container) const {
        if (!a_container) return false;
        return ShouldPullFrom(a_container->GetFormID());
    }

    bool ContainerRegistry::ShouldPullFrom(RE::FormID a_formID) const {
        // Check global INI status first (separate mutex)
        bool isINIGlobal = IsINIGlobalContainer(a_formID);

        std::lock_guard lock(m_overridesMutex);

        // Check player overrides first
        auto it = m_playerOverrides.find(a_formID);
        if (it != m_playerOverrides.end()) {
            // Player has an override for this container
            return it->second.state != ContainerState::kOff;
        }

        // No override - check INI defaults
        // Global containers are active by default (if globals enabled)
        if (isINIGlobal && INISettings::GetSingleton()->GetEnableGlobalContainers()) {
            return true;
        }

        // Fall back to local INI configuration (if enabled)
        if (INISettings::GetSingleton()->GetEnableINIContainers()) {
            std::lock_guard iniLock(m_iniMutex);
            return m_iniConfigured.contains(a_formID);
        }
        return false;
    }

    int ContainerRegistry::GetContainerState(RE::FormID a_formID) const {
        // Check for player override
        {
            std::lock_guard lock(m_overridesMutex);
            auto it = m_playerOverrides.find(a_formID);
            if (it != m_playerOverrides.end()) {
                return static_cast<int>(it->second.state);
            }
        }

        // No override - return INI baseline
        return static_cast<int>(GetINIBaselineState(a_formID));
    }

    ContainerState ContainerRegistry::GetINIBaselineState(RE::FormID a_formID) const {
        // Check global first
        {
            std::lock_guard lock(m_globalMutex);
            if (m_globalContainers.contains(a_formID)) {
                if (INISettings::GetSingleton()->GetEnableGlobalContainers()) {
                    return ContainerState::kGlobal;
                }
            }
        }

        // Check local INI (if enabled)
        if (INISettings::GetSingleton()->GetEnableINIContainers()) {
            std::lock_guard lock(m_iniMutex);
            if (m_iniConfigured.contains(a_formID)) {
                return ContainerState::kLocal;
            }
        }

        return ContainerState::kOff;
    }

    std::pair<std::string, std::string> ContainerRegistry::CaptureContainerMetadata(RE::TESObjectREFR* a_ref) {
        std::string name;
        std::string location;

        if (!a_ref) return {name, location};

        // Get container name from base object
        auto* baseObj = a_ref->GetBaseObject();
        if (baseObj) {
            const char* baseName = baseObj->GetName();
            if (baseName && baseName[0]) {
                name = baseName;
            }
        }

        // Get cell/location name - prefer display name over editor ID for consistency
        auto* cell = a_ref->GetParentCell();
        if (cell) {
            const char* fullName = cell->GetName();
            if (fullName && fullName[0]) {
                location = fullName;
            } else {
                const char* cellName = cell->GetFormEditorID();
                if (cellName && cellName[0]) {
                    location = cellName;
                }
            }
        }

        return {name, location};
    }

    int ContainerRegistry::ToggleCycleState(RE::FormID a_formID, RE::TESObjectREFR* a_ref) {
        std::lock_guard lock(m_overridesMutex);

        // Determine current effective state
        ContainerState currentState;

        auto it = m_playerOverrides.find(a_formID);
        if (it != m_playerOverrides.end()) {
            currentState = it->second.state;
        } else {
            // No override - use INI baseline (but we can't lock other mutexes while holding m_overridesMutex,
            // so we need to check inline)
            bool isINIGlobal = false;
            {
                // Note: This is safe because m_globalContainers is only written during Initialize()
                // which happens before any toggling can occur
                isINIGlobal = m_globalContainers.contains(a_formID);
            }
            bool isINILocal = m_iniConfigured.contains(a_formID);

            if (isINIGlobal && INISettings::GetSingleton()->GetEnableGlobalContainers()) {
                currentState = ContainerState::kGlobal;
            } else if (isINILocal) {
                currentState = ContainerState::kLocal;
            } else {
                currentState = ContainerState::kOff;
            }
        }

        // Advance to next state in cycle: off -> local -> global -> off
        ContainerState newState;
        switch (currentState) {
            case ContainerState::kOff:
                newState = ContainerState::kLocal;
                break;
            case ContainerState::kLocal:
                newState = ContainerState::kGlobal;
                break;
            case ContainerState::kGlobal:
                newState = ContainerState::kOff;
                break;
            default:
                newState = ContainerState::kLocal;
                break;
        }

        // Always keep the override so the container stays visible in MCM
        if (it != m_playerOverrides.end()) {
            it->second.state = newState;
        } else {
            // New override - capture metadata
            ContainerInfo info;
            info.formID = a_formID;
            info.state = newState;

            if (a_ref) {
                auto [name, location] = CaptureContainerMetadata(a_ref);
                info.name = std::move(name);
                info.location = std::move(location);
            }

            m_playerOverrides[a_formID] = std::move(info);
        }
        logger::info("Container {:08X} set to state {} by player", a_formID, static_cast<int>(newState));

        return static_cast<int>(newState);
    }

    bool ContainerRegistry::IsINIConfigured(RE::FormID a_formID) const {
        std::lock_guard lock(m_iniMutex);
        return m_iniConfigured.contains(a_formID);
    }

    bool ContainerRegistry::IsManuallyEnabled(RE::FormID a_formID) const {
        std::lock_guard lock(m_overridesMutex);
        auto it = m_playerOverrides.find(a_formID);
        if (it != m_playerOverrides.end()) {
            return it->second.state == ContainerState::kLocal || it->second.state == ContainerState::kGlobal;
        }
        return false;
    }

    bool ContainerRegistry::IsManuallyDisabled(RE::FormID a_formID) const {
        std::lock_guard lock(m_overridesMutex);
        auto it = m_playerOverrides.find(a_formID);
        if (it != m_playerOverrides.end()) {
            return it->second.state == ContainerState::kOff;
        }
        return false;
    }

    void ContainerRegistry::ClearPlayerOverrides() {
        std::lock_guard lock(m_overridesMutex);
        m_playerOverrides.clear();
        logger::info("Cleared all player container overrides");
    }

    std::size_t ContainerRegistry::GetINIConfiguredCount() const {
        std::lock_guard lock(m_iniMutex);
        return m_iniConfigured.size();
    }

    std::size_t ContainerRegistry::GetManuallyEnabledCount() const {
        std::lock_guard lock(m_overridesMutex);
        std::size_t count = 0;
        for (const auto& [formID, info] : m_playerOverrides) {
            if (info.state == ContainerState::kLocal || info.state == ContainerState::kGlobal) {
                count++;
            }
        }
        return count;
    }

    std::size_t ContainerRegistry::GetManuallyDisabledCount() const {
        std::lock_guard lock(m_overridesMutex);
        std::size_t count = 0;
        for (const auto& [formID, info] : m_playerOverrides) {
            if (info.state == ContainerState::kOff) {
                count++;
            }
        }
        return count;
    }

    void ContainerRegistry::DumpContainersToLog() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::error("[SCIE] DumpContainers: No player");
            return;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            logger::error("[SCIE] DumpContainers: No current cell");
            return;
        }

        auto playerPos = player->GetPosition();
        float maxRadius = INISettings::GetSingleton()->GetMaxContainerDistance();

        logger::info("[SCIE] === Container Dump ===");
        logger::info("[SCIE] Cell: {} | Radius: {:.0f} units",
            cell->GetFormEditorID() ? cell->GetFormEditorID() : "unnamed",
            maxRadius);

        std::uint32_t count = 0;

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Get plugin info
            auto [plugin, localID] = GetPluginInfo(&ref);

            // Get EditorID
            const char* editorID = ref.GetFormEditorID();
            std::string editorIDStr = editorID ? editorID : "(no EditorID)";

            // Get base form name
            const char* baseName = baseObj->GetName();
            std::string baseNameStr = baseName && baseName[0] ? baseName : "Container";

            // Check status
            bool isLocked = ref.IsLocked();
            bool wouldSteal = false;

            auto* owner = ref.GetOwner();
            if (owner) {
                if (owner != player->GetActorBase()) {
                    if (auto* faction = owner->As<RE::TESFaction>()) {
                        wouldSteal = !player->IsInFaction(faction);
                    } else {
                        wouldSteal = true;
                    }
                }
            }

            // Check if already configured
            bool isConfigured = ShouldPullFrom(&ref);

            logger::info("[SCIE] {}|0x{:06X} | {} | {} | {:.0f}u | {} | {} | {}",
                plugin,
                localID,
                editorIDStr,
                baseNameStr,
                distance,
                isLocked ? "LOCKED" : "Unlocked",
                wouldSteal ? "STEAL" : "Owned",
                isConfigured ? "ACTIVE" : "-");

            count++;
            return RE::BSContainer::ForEachResult::kContinue;
        });

        logger::info("[SCIE] === End Dump ({} containers) ===", count);
    }

    std::string ContainerRegistry::GenerateSnapshot() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::error("[SCIE] GenerateSnapshot: No player");
            return "";
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            logger::error("[SCIE] GenerateSnapshot: No current cell");
            return "";
        }

        auto playerPos = player->GetPosition();
        float maxRadius = INISettings::GetSingleton()->GetMaxContainerDistance();

        // Collect player-enabled containers in current cell
        std::vector<std::tuple<std::string, std::uint32_t, std::string>> entries;  // plugin, localID, editorID

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Only include player-enabled containers (not INI-configured)
            if (!IsManuallyEnabled(ref.GetFormID())) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            auto [plugin, localID] = GetPluginInfo(&ref);
            const char* editorID = ref.GetFormEditorID();
            std::string editorIDStr = editorID ? editorID : "";

            entries.emplace_back(plugin, localID, editorIDStr);
            return RE::BSContainer::ForEachResult::kContinue;
        });

        if (entries.empty()) {
            logger::info("[SCIE] GenerateSnapshot: No player-marked containers in cell");
            return "";
        }

        // Generate filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &time);

        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);

        auto outputDir = std::filesystem::path("Data/SKSE/Plugins/CraftingInventoryExtender");
        std::filesystem::create_directories(outputDir);

        auto outputPath = outputDir / fmt::format("SCIE_Snapshot_{}.ini", timestamp);

        // Write the file
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            logger::error("[SCIE] GenerateSnapshot: Failed to create {}", outputPath.string());
            return "";
        }

        // Header
        char dateStr[64];
        std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &tm);

        file << "; SCIE Container Snapshot\n";
        file << "; Generated: " << dateStr << "\n";
        file << "; Location: " << (cell->GetFormEditorID() ? cell->GetFormEditorID() : "Unknown") << "\n";
        file << "; Cell FormID: " << fmt::format("{:08X}", cell->GetFormID()) << "\n";
        file << ";\n";
        file << "; Copy this file to Data/SKSE/Plugins/CraftingInventoryExtender/\n";
        file << "; to make these containers permanently enabled.\n";
        file << "\n";
        file << "[Containers]\n";

        for (const auto& [plugin, localID, editorID] : entries) {
            if (!editorID.empty()) {
                file << fmt::format("{}|0x{:06X} = true    ; {}\n", plugin, localID, editorID);
            } else {
                file << fmt::format("{}|0x{:06X} = true\n", plugin, localID);
            }
        }

        file.close();

        logger::info("[SCIE] GenerateSnapshot: Saved {} containers to {}", entries.size(), outputPath.string());
        return outputPath.string();
    }

    std::int32_t ContainerRegistry::ClearOverridesInCell(float a_maxDistance) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::error("[SCIE] ClearOverridesInCell: No player");
            return 0;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            logger::error("[SCIE] ClearOverridesInCell: No current cell");
            return 0;
        }

        auto playerPos = player->GetPosition();
        float maxRadius = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();

        // Collect FormIDs of containers in cell
        std::vector<RE::FormID> containerIDs;

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            containerIDs.push_back(ref.GetFormID());
            return RE::BSContainer::ForEachResult::kContinue;
        });

        // Clear overrides for these containers
        std::int32_t clearedCount = 0;
        {
            std::lock_guard lock(m_overridesMutex);
            for (RE::FormID formID : containerIDs) {
                if (m_playerOverrides.erase(formID) > 0) {
                    clearedCount++;
                }
            }
        }

        logger::info("[SCIE] ClearOverridesInCell: Cleared {} overrides from {} containers",
            clearedCount, containerIDs.size());
        return clearedCount;
    }

    std::vector<RE::TESObjectREFR*> ContainerRegistry::GetEnabledContainersInCell(float a_maxDistance) {
        std::vector<RE::TESObjectREFR*> result;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return result;

        auto* cell = player->GetParentCell();
        if (!cell) return result;

        auto playerPos = player->GetPosition();
        float maxRadius = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            RE::FormID refFormID = ref.GetFormID();

            // Get the effective state
            int state = GetContainerState(refFormID);

            // Include local and global containers (but not off)
            if (state == static_cast<int>(ContainerState::kLocal) ||
                state == static_cast<int>(ContainerState::kGlobal)) {
                result.push_back(&ref);
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        logger::debug("GetEnabledContainersInCell: found {} containers within {} units",
            result.size(), maxRadius);
        return result;
    }

    std::vector<RE::TESObjectREFR*> ContainerRegistry::GetDisabledContainersInCell(float a_maxDistance) {
        std::vector<RE::TESObjectREFR*> result;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return result;

        auto* cell = player->GetParentCell();
        if (!cell) return result;

        auto playerPos = player->GetPosition();
        float maxRadius = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Check if this container was explicitly disabled by player
            if (IsManuallyDisabled(ref.GetFormID())) {
                result.push_back(&ref);
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        logger::debug("GetDisabledContainersInCell: found {} containers within {} units",
            result.size(), maxRadius);
        return result;
    }

    std::vector<RE::TESObjectREFR*> ContainerRegistry::GetGlobalContainersInCell(float a_maxDistance) {
        std::vector<RE::TESObjectREFR*> result;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return result;

        auto* cell = player->GetParentCell();
        if (!cell) return result;

        auto playerPos = player->GetPosition();
        float maxRadius = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();

        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* baseObj = ref.GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            float distance = playerPos.GetDistance(ref.GetPosition());
            if (distance > maxRadius) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            RE::FormID refFormID = ref.GetFormID();
            int state = GetContainerState(refFormID);

            if (state == static_cast<int>(ContainerState::kGlobal)) {
                result.push_back(&ref);
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        logger::debug("GetGlobalContainersInCell: found {} containers within {} units",
            result.size(), maxRadius);
        return result;
    }

    bool ContainerRegistry::IsGlobalContainer(RE::FormID a_formID) const {
        // Check player override first
        {
            std::lock_guard lock(m_overridesMutex);
            auto it = m_playerOverrides.find(a_formID);
            if (it != m_playerOverrides.end()) {
                return it->second.state == ContainerState::kGlobal;
            }
        }

        // Check INI global
        return IsINIGlobalContainer(a_formID);
    }

    bool ContainerRegistry::IsINIGlobalContainer(RE::FormID a_formID) const {
        std::lock_guard lock(m_globalMutex);
        return m_globalContainers.contains(a_formID);
    }

    std::vector<RE::TESObjectREFR*> ContainerRegistry::GetGlobalContainers() const {
        std::vector<RE::TESObjectREFR*> result;

        // Check if global containers are enabled
        if (!INISettings::GetSingleton()->GetEnableGlobalContainers()) {
            logger::debug("GetGlobalContainers: Global containers disabled in settings");
            return result;
        }

        // Collect all FormIDs that should be treated as global
        std::unordered_set<RE::FormID> globalFormIDs;

        // INI-configured globals
        {
            std::lock_guard lock(m_globalMutex);
            globalFormIDs = m_globalContainers;
        }

        // Player-promoted globals
        {
            std::lock_guard lock(m_overridesMutex);
            for (const auto& [formID, info] : m_playerOverrides) {
                if (info.state == ContainerState::kGlobal) {
                    globalFormIDs.insert(formID);
                } else if (info.state == ContainerState::kOff) {
                    // Player disabled an INI global - remove it
                    globalFormIDs.erase(formID);
                }
                // kLocal override on an INI global -> not global anymore (local only)
                else if (info.state == ContainerState::kLocal && globalFormIDs.contains(formID)) {
                    globalFormIDs.erase(formID);
                }
            }
        }

        for (RE::FormID formID : globalFormIDs) {
            // Resolve the FormID to an actual reference
            auto* form = RE::TESForm::LookupByID(formID);
            if (!form) {
                logger::debug("Global container {:08X} not found", formID);
                continue;
            }

            auto* ref = form->As<RE::TESObjectREFR>();
            if (!ref) {
                logger::warn("Global container {:08X} is not a reference", formID);
                continue;
            }

            // Verify it's actually a container
            auto* baseObj = ref->GetBaseObject();
            if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
                logger::warn("Global container {:08X} base object is not a container", formID);
                continue;
            }

            // Skip locked containers
            if (ref->IsLocked()) {
                logger::debug("Global container {:08X} skipped (locked)", formID);
                continue;
            }

            result.push_back(ref);
            logger::debug("Global container {:08X} added to session", formID);
        }

        logger::info("GetGlobalContainers: {} containers available", result.size());
        return result;
    }

    std::size_t ContainerRegistry::GetGlobalContainerCount() const {
        std::size_t count = 0;

        // Count INI globals
        {
            std::lock_guard lock(m_globalMutex);
            count = m_globalContainers.size();
        }

        // Add player-promoted globals, subtract disabled ones
        {
            std::lock_guard lock(m_overridesMutex);
            for (const auto& [formID, info] : m_playerOverrides) {
                bool isINIGlobal = m_globalContainers.contains(formID);
                if (info.state == ContainerState::kGlobal && !isINIGlobal) {
                    count++;
                } else if (info.state != ContainerState::kGlobal && isINIGlobal) {
                    count--;
                }
            }
        }

        return count;
    }

    std::unordered_map<RE::FormID, ContainerInfo> ContainerRegistry::GetPlayerOverrides() const {
        std::lock_guard lock(m_overridesMutex);
        return m_playerOverrides;
    }

    std::size_t ContainerRegistry::GetPlayerOverrideCount() const {
        std::lock_guard lock(m_overridesMutex);
        return m_playerOverrides.size();
    }

    std::vector<ContainerInfo> ContainerRegistry::GetSortedOverrides() const {
        std::lock_guard lock(m_overridesMutex);

        std::vector<ContainerInfo> sorted;
        sorted.reserve(m_playerOverrides.size());

        for (const auto& [formID, info] : m_playerOverrides) {
            sorted.push_back(info);
        }

        // Sort by display location (or location, or ESP fallback), then by name
        std::sort(sorted.begin(), sorted.end(), [](const ContainerInfo& a, const ContainerInfo& b) {
            // Use displayLocation if location is empty
            const std::string& locA = a.location.empty() ? a.displayLocation : a.location;
            const std::string& locB = b.location.empty() ? b.displayLocation : b.location;

            if (locA != locB) return locA < locB;
            return a.name < b.name;
        });

        return sorted;
    }

    void ContainerRegistry::SetContainerState(RE::FormID a_formID, ContainerState a_newState) {
        std::lock_guard lock(m_overridesMutex);

        auto it = m_playerOverrides.find(a_formID);
        if (it != m_playerOverrides.end()) {
            it->second.state = a_newState;
        } else {
            ContainerInfo info;
            info.formID = a_formID;
            info.state = a_newState;
            m_playerOverrides[a_formID] = std::move(info);
        }
        logger::info("Container {:08X} set to state {}", a_formID, static_cast<int>(a_newState));
    }

    void ContainerRegistry::SetContainerStateByIndex(int a_index, ContainerState a_newState) {
        auto sorted = GetSortedOverrides();
        if (a_index < 0 || a_index >= static_cast<int>(sorted.size())) {
            logger::warn("SetContainerStateByIndex: index {} out of range (size={})", a_index, sorted.size());
            return;
        }

        // MCM path: always keep the override (don't erase when matching INI baseline)
        // so the entry stays visible on the Containers page
        RE::FormID formID = sorted[a_index].formID;
        std::lock_guard lock(m_overridesMutex);
        auto it = m_playerOverrides.find(formID);
        if (it != m_playerOverrides.end()) {
            it->second.state = a_newState;
            logger::info("Container {:08X} set to state {} via MCM", formID, static_cast<int>(a_newState));
        } else {
            logger::warn("SetContainerStateByIndex: FormID {:08X} not in overrides", formID);
        }
    }

    std::vector<std::pair<std::string, std::vector<ContainerInfo>>> ContainerRegistry::GetContainersGroupedByLocation() const {
        auto sorted = GetSortedOverrides();

        std::vector<std::pair<std::string, std::vector<ContainerInfo>>> groups;
        std::string currentLocation;

        for (auto& info : sorted) {
            const std::string& loc = info.location.empty() ? info.displayLocation : info.location;
            std::string displayLoc = loc.empty() ? "Unknown Location" : loc;

            if (displayLoc != currentLocation) {
                groups.emplace_back(displayLoc, std::vector<ContainerInfo>{});
                currentLocation = displayLoc;
            }
            groups.back().second.push_back(info);
        }

        return groups;
    }

    int ContainerRegistry::RemovePlayerContainersByLocation(const std::string& a_location) {
        std::lock_guard lock(m_overridesMutex);

        // Collect FormIDs to remove (matching effective display location)
        std::vector<RE::FormID> toRemove;
        for (const auto& [formID, info] : m_playerOverrides) {
            const std::string& loc = info.location.empty() ? info.displayLocation : info.location;
            std::string displayLoc = loc.empty() ? "Unknown Location" : loc;
            if (displayLoc == a_location) {
                toRemove.push_back(formID);
            }
        }

        for (auto formID : toRemove) {
            m_playerOverrides.erase(formID);
        }

        logger::info("Removed {} player container overrides for location '{}'", toRemove.size(), a_location);
        return static_cast<int>(toRemove.size());
    }

    ContainerRegistry::PlayerContainerPage ContainerRegistry::GetPlayerContainerPage(int a_page, int a_pageSize) const {
        PlayerContainerPage result;
        auto sorted = GetSortedOverrides();

        int startIdx = a_page * a_pageSize;
        int endIdx = std::min(startIdx + a_pageSize, static_cast<int>(sorted.size()));

        if (startIdx >= static_cast<int>(sorted.size())) {
            return result;
        }

        for (int i = startIdx; i < endIdx; ++i) {
            const auto& info = sorted[i];
            std::string name = info.name.empty() ? fmt::format("Container {:08X}", info.formID) : info.name;

            // Check if container has the respawn flag (unsafe for permanent storage)
            auto* form = RE::TESForm::LookupByID(info.formID);
            if (form) {
                auto* ref = form->As<RE::TESObjectREFR>();
                if (ref && IsContainerUnsafe(ref)) {
                    name += TranslationService::GetSingleton()->GetTranslation("$SCIE_SuffixUnsafe");
                }
            }

            result.names.push_back(name);

            const std::string& loc = info.location.empty() ? info.displayLocation : info.location;
            result.locations.push_back(loc.empty() ? "Unknown" : loc);

            result.states.push_back(static_cast<int>(info.state));
        }

        return result;
    }

    void ContainerRegistry::BackfillOverrideMetadata() {
        std::lock_guard lock(m_overridesMutex);

        for (auto& [formID, info] : m_playerOverrides) {
            // Try to backfill name
            if (info.name.empty()) {
                auto* form = RE::TESForm::LookupByID(formID);
                if (form) {
                    auto* ref = form->As<RE::TESObjectREFR>();
                    if (ref) {
                        auto* baseObj = ref->GetBaseObject();
                        if (baseObj) {
                            const char* baseName = baseObj->GetName();
                            if (baseName && baseName[0]) {
                                info.name = baseName;
                            }
                        }

                        // Try to get cell name if location is empty
                        // Prefer display name over editor ID for consistency with CaptureContainerMetadata
                        if (info.location.empty()) {
                            auto* cell = ref->GetParentCell();
                            if (cell) {
                                const char* fullName = cell->GetName();
                                if (fullName && fullName[0]) {
                                    info.location = fullName;
                                } else {
                                    const char* cellName = cell->GetFormEditorID();
                                    if (cellName && cellName[0]) {
                                        info.location = cellName;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // If location is still empty, use ESP filename as transient fallback
            if (info.location.empty()) {
                info.displayLocation = GetESPFilename(formID);
            }
        }

        logger::info("BackfillOverrideMetadata: processed {} overrides", m_playerOverrides.size());
    }

    bool ContainerRegistry::IsLOTDInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        // Check for the main LOTD plugin
        return dataHandler->LookupModByName("LegacyoftheDragonborn.esm") != nullptr;
    }

    bool ContainerRegistry::IsGeneralStoresInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        return dataHandler->LookupModByName("GeneralStores.esm") != nullptr;
    }

    bool ContainerRegistry::IsLinkedCraftingStorageInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        return dataHandler->LookupModByName("LinkedCraftingStorage.esp") != nullptr;
    }

    bool ContainerRegistry::IsConvenientHorsesInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        return dataHandler->LookupModByName("Convenient Horses.esp") != nullptr;
    }

    bool ContainerRegistry::IsHipBagInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        return dataHandler->LookupModByName("HipBag.esp") != nullptr;
    }

    bool ContainerRegistry::IsNFFInstalled() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return false;

        return dataHandler->LookupModByName("nwsFollowerFramework.esp") != nullptr;
    }

    bool ContainerRegistry::IsEssentialFavoritesInstalled() {
        // Check for the SKSE plugin DLL
        auto pluginPath = std::filesystem::path("Data/SKSE/Plugins/po3_EssentialFavorites.dll");
        return std::filesystem::exists(pluginPath);
    }

    bool ContainerRegistry::IsFavoriteMiscItemsInstalled() {
        // Check for the SKSE plugin DLL
        auto pluginPath = std::filesystem::path("Data/SKSE/Plugins/po3_FavoriteMiscItems.dll");
        return std::filesystem::exists(pluginPath);
    }

    std::size_t ContainerRegistry::GetFavoritedItemsExcludedCount() {
        return Hooks::g_craftingSession.favoritedItemsExcluded;
    }

    void ContainerRegistry::LoadINIFiles() {
        auto configDir = std::filesystem::path("Data/SKSE/Plugins/CraftingInventoryExtender");

        if (!std::filesystem::exists(configDir)) {
            logger::info("Config directory not found: {} - no preset containers", configDir.string());
            return;
        }

        logger::info("Loading container configs from {}", configDir.string());

        std::size_t fileCount = 0;

        // Collect and sort INI files alphabetically so load order is deterministic.
        // A user's "SCIE_ZZZ_Overrides.ini" will always load after "SCIE_VanillaHomes.ini",
        // allowing =false entries to override earlier =true entries.
        std::vector<std::filesystem::path> iniFiles;
        for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                iniFiles.push_back(entry.path());
            }
        }
        std::sort(iniFiles.begin(), iniFiles.end());

        for (const auto& iniPath : iniFiles) {
            ParseINIFile(iniPath);
            fileCount++;
        }

        logger::info("Processed {} INI files: {} local containers, {} global containers",
            fileCount, m_iniConfigured.size(), m_globalContainers.size());
    }

    void ContainerRegistry::ParseINIFile(const std::filesystem::path& a_path) {
        logger::debug("Parsing container config: {}", a_path.string());

        std::ifstream file(a_path);
        if (!file.is_open()) {
            logger::warn("Failed to open config file: {}", a_path.string());
            return;
        }

        std::string line;
        bool inContainersSection = false;
        bool inGlobalSection = false;

        while (std::getline(file, line)) {
            // Trim leading whitespace
            auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            // Trim trailing whitespace/newlines
            auto end = line.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }

            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            // Check for section header
            if (line[0] == '[') {
                inContainersSection = (line.find("[Containers]") != std::string::npos);
                inGlobalSection = (line.find("[GlobalContainers]") != std::string::npos);
                continue;
            }

            // Parse key = value in either section
            if (inContainersSection || inGlobalSection) {
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
                    }
                };
                trimStr(key);
                trimStr(value);

                // Strip inline comments from value (everything after ; or #)
                auto commentPos = value.find(';');
                if (commentPos != std::string::npos) {
                    value = value.substr(0, commentPos);
                    trimStr(value);
                }
                commentPos = value.find('#');
                if (commentPos != std::string::npos) {
                    value = value.substr(0, commentPos);
                    trimStr(value);
                }

                // Check if value is "true" or "false" (case insensitive)
                std::string valueLower = value;
                std::transform(valueLower.begin(), valueLower.end(), valueLower.begin(), ::tolower);
                bool enabled = (valueLower == "true" || valueLower == "1");
                bool disabled = (valueLower == "false" || valueLower == "0");

                if (!enabled && !disabled) {
                    continue;  // Unrecognized value, skip
                }

                auto formID = ParseContainerEntry(key, value);
                if (formID) {
                    if (enabled) {
                        if (inGlobalSection) {
                            std::lock_guard lock(m_globalMutex);
                            m_globalContainers.insert(*formID);
                            logger::info("Global container configured: {:08X}", *formID);
                        } else {
                            std::lock_guard lock(m_iniMutex);
                            m_iniConfigured.insert(*formID);
                            logger::debug("INI configured container: {:08X}", *formID);
                        }
                    } else {
                        // =false actively removes a container added by an earlier INI
                        if (inGlobalSection) {
                            std::lock_guard lock(m_globalMutex);
                            if (m_globalContainers.erase(*formID)) {
                                logger::info("Global container disabled by INI override: {:08X}", *formID);
                            }
                        } else {
                            std::lock_guard lock(m_iniMutex);
                            if (m_iniConfigured.erase(*formID)) {
                                logger::debug("INI container disabled by override: {:08X}", *formID);
                            }
                        }
                    }
                }
            }
        }
    }

    std::optional<RE::FormID> ContainerRegistry::ParseContainerEntry(
        const std::string& a_key,
        [[maybe_unused]] const std::string& a_value)
    {
        // Expected format: "Plugin.esp|FormID" or "Plugin.esp|EditorID"
        auto pipePos = a_key.find('|');
        if (pipePos == std::string::npos) {
            logger::warn("Invalid container entry (no '|'): {}", a_key);
            return std::nullopt;
        }

        std::string plugin = a_key.substr(0, pipePos);
        std::string identifier = a_key.substr(pipePos + 1);

        if (plugin.empty() || identifier.empty()) {
            logger::warn("Invalid container entry (empty plugin or ID): {}", a_key);
            return std::nullopt;
        }

        // Determine if identifier is FormID or EditorID
        // FormID: starts with "0x" or "0X", or is all hex digits
        bool isFormID = false;
        if (identifier.length() >= 2 &&
            (identifier.substr(0, 2) == "0x" || identifier.substr(0, 2) == "0X")) {
            isFormID = true;
        } else {
            // Check if all characters are hex digits
            isFormID = std::all_of(identifier.begin(), identifier.end(), ::isxdigit);
        }

        if (isFormID) {
            // Parse as FormID
            try {
                std::uint32_t localFormID = static_cast<std::uint32_t>(
                    std::stoul(identifier, nullptr, 16));
                return ResolveFormID(plugin, localFormID);
            } catch (const std::exception& e) {
                logger::warn("Failed to parse FormID '{}': {}", identifier, e.what());
                return std::nullopt;
            }
        } else {
            // It's an EditorID
            return ResolveEditorID(plugin, identifier);
        }
    }

    std::optional<RE::FormID> ContainerRegistry::ResolveFormID(
        const std::string& a_plugin,
        std::uint32_t a_localFormID)
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::error("TESDataHandler not available");
            return std::nullopt;
        }

        auto* modInfo = dataHandler->LookupModByName(a_plugin);
        if (!modInfo) {
            logger::debug("Plugin '{}' not loaded, skipping", a_plugin);
            return std::nullopt;
        }

        RE::FormID fullFormID;
        if (modInfo->IsLight()) {
            // Light plugin: 0xFEXXXYYY where XXX is light index, YYY is local ID
            auto lightIndex = modInfo->GetSmallFileCompileIndex();
            fullFormID = (0xFE000000) |
                         (static_cast<RE::FormID>(lightIndex) << 12) |
                         (a_localFormID & 0x00000FFF);
        } else {
            // Regular plugin: 0xXXYYYYYY where XX is mod index
            auto modIndex = modInfo->GetCompileIndex();
            fullFormID = (static_cast<RE::FormID>(modIndex) << 24) |
                         (a_localFormID & 0x00FFFFFF);
        }

        logger::debug("Resolved {}|{:X} -> {:08X}", a_plugin, a_localFormID, fullFormID);
        return fullFormID;
    }

    std::optional<RE::FormID> ContainerRegistry::ResolveEditorID(
        const std::string& a_plugin,
        const std::string& a_editorID)
    {
        // Check cache first
        std::string cacheKey = a_plugin + "|" + a_editorID;
        {
            std::lock_guard lock(m_editorIDMutex);
            auto it = m_editorIDCache.find(cacheKey);
            if (it != m_editorIDCache.end()) {
                return it->second;
            }
        }

        // Try to resolve now
        auto* form = RE::TESForm::LookupByEditorID(a_editorID);
        if (form) {
            // Verify it belongs to the expected plugin
            // (For now, just trust the EditorID lookup)
            RE::FormID formID = form->GetFormID();

            std::lock_guard lock(m_editorIDMutex);
            m_editorIDCache[cacheKey] = formID;

            logger::debug("Resolved {}|{} -> {:08X}", a_plugin, a_editorID, formID);
            return formID;
        }

        // Store as pending for later resolution
        {
            std::lock_guard lock(m_editorIDMutex);
            m_pendingEditorIDs.push_back({a_plugin, a_editorID});
        }

        logger::debug("EditorID '{}' not found yet, deferred", a_editorID);
        return std::nullopt;
    }

    // Cosave callbacks

    void ContainerRegistry::OnGameSaved(SKSE::SerializationInterface* a_intfc) {
        GetSingleton()->SaveToCoSave(a_intfc);
    }

    void ContainerRegistry::OnGameLoaded(SKSE::SerializationInterface* a_intfc) {
        std::uint32_t type, version, length;

        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kCosaveID) {
                GetSingleton()->LoadFromCoSave(a_intfc, version);
            }
        }

        // Backfill metadata for migrated/loaded overrides (name, location, ESP fallback)
        GetSingleton()->BackfillOverrideMetadata();
    }

    void ContainerRegistry::OnRevert(SKSE::SerializationInterface* a_intfc) {
        GetSingleton()->Revert(a_intfc);
    }

    void ContainerRegistry::SaveToCoSave(SKSE::SerializationInterface* a_intfc) {
        if (!a_intfc->OpenRecord(kCosaveID, kCosaveVersion)) {
            logger::error("Failed to open cosave record");
            return;
        }

        std::lock_guard lock(m_overridesMutex);

        // Write override count
        std::uint32_t overrideCount = static_cast<std::uint32_t>(m_playerOverrides.size());
        a_intfc->WriteRecordData(&overrideCount, sizeof(overrideCount));

        for (const auto& [formID, info] : m_playerOverrides) {
            // Write FormID
            a_intfc->WriteRecordData(&formID, sizeof(formID));

            // Write state
            std::uint8_t state = static_cast<std::uint8_t>(info.state);
            a_intfc->WriteRecordData(&state, sizeof(state));

            // Write name (length-prefixed)
            std::uint16_t nameLen = static_cast<std::uint16_t>(info.name.size());
            a_intfc->WriteRecordData(&nameLen, sizeof(nameLen));
            if (nameLen > 0) {
                a_intfc->WriteRecordData(info.name.data(), nameLen);
            }

            // Write location (length-prefixed)
            std::uint16_t locLen = static_cast<std::uint16_t>(info.location.size());
            a_intfc->WriteRecordData(&locLen, sizeof(locLen));
            if (locLen > 0) {
                a_intfc->WriteRecordData(info.location.data(), locLen);
            }
        }

        logger::info("Saved {} container overrides (cosave v{})", overrideCount, kCosaveVersion);
    }

    void ContainerRegistry::LoadFromCoSave(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version) {
        std::lock_guard lock(m_overridesMutex);
        m_playerOverrides.clear();

        if (a_version == 1) {
            // === MIGRATION: v1 format (old enabled/disabled sets) ===
            logger::info("Migrating cosave from v1 to v2 format...");

            // Read enabled containers
            std::uint32_t enabledCount = 0;
            if (!a_intfc->ReadRecordData(&enabledCount, sizeof(enabledCount))) {
                logger::error("Failed to read enabled count from v1 cosave");
                return;
            }

            for (std::uint32_t i = 0; i < enabledCount; ++i) {
                RE::FormID oldFormID = 0;
                if (!a_intfc->ReadRecordData(&oldFormID, sizeof(oldFormID))) {
                    logger::error("Failed to read enabled FormID {} from v1 cosave", i);
                    return;
                }

                RE::FormID newFormID = 0;
                if (a_intfc->ResolveFormID(oldFormID, newFormID)) {
                    ContainerInfo info;
                    info.formID = newFormID;
                    info.state = ContainerState::kLocal;  // Old "enabled" = local
                    // name and location empty - will be backfilled
                    m_playerOverrides[newFormID] = std::move(info);
                } else {
                    logger::debug("Failed to resolve v1 enabled container {:08X}", oldFormID);
                }
            }

            // Read disabled containers
            std::uint32_t disabledCount = 0;
            if (!a_intfc->ReadRecordData(&disabledCount, sizeof(disabledCount))) {
                logger::error("Failed to read disabled count from v1 cosave");
                return;
            }

            for (std::uint32_t i = 0; i < disabledCount; ++i) {
                RE::FormID oldFormID = 0;
                if (!a_intfc->ReadRecordData(&oldFormID, sizeof(oldFormID))) {
                    logger::error("Failed to read disabled FormID {} from v1 cosave", i);
                    return;
                }

                RE::FormID newFormID = 0;
                if (a_intfc->ResolveFormID(oldFormID, newFormID)) {
                    ContainerInfo info;
                    info.formID = newFormID;
                    info.state = ContainerState::kOff;  // Old "disabled" = off
                    m_playerOverrides[newFormID] = std::move(info);
                } else {
                    logger::debug("Failed to resolve v1 disabled container {:08X}", oldFormID);
                }
            }

            logger::info("Migrated {} overrides from cosave v1 to v2 ({} enabled, {} disabled)",
                m_playerOverrides.size(), enabledCount, disabledCount);

        } else if (a_version == 2) {
            // === v2 format ===
            std::uint32_t overrideCount = 0;
            if (!a_intfc->ReadRecordData(&overrideCount, sizeof(overrideCount))) {
                logger::error("Failed to read override count from v2 cosave");
                return;
            }

            for (std::uint32_t i = 0; i < overrideCount; ++i) {
                // Read FormID
                RE::FormID oldFormID = 0;
                if (!a_intfc->ReadRecordData(&oldFormID, sizeof(oldFormID))) {
                    logger::error("Failed to read FormID {} from v2 cosave", i);
                    return;
                }

                // Read state
                std::uint8_t stateVal = 0;
                if (!a_intfc->ReadRecordData(&stateVal, sizeof(stateVal))) {
                    logger::error("Failed to read state {} from v2 cosave", i);
                    return;
                }

                // Read name
                std::uint16_t nameLen = 0;
                if (!a_intfc->ReadRecordData(&nameLen, sizeof(nameLen))) {
                    logger::error("Failed to read name length {} from v2 cosave", i);
                    return;
                }
                std::string name;
                if (nameLen > 0) {
                    name.resize(nameLen);
                    if (!a_intfc->ReadRecordData(name.data(), nameLen)) {
                        logger::error("Failed to read name data {} from v2 cosave", i);
                        return;
                    }
                }

                // Read location
                std::uint16_t locLen = 0;
                if (!a_intfc->ReadRecordData(&locLen, sizeof(locLen))) {
                    logger::error("Failed to read location length {} from v2 cosave", i);
                    return;
                }
                std::string location;
                if (locLen > 0) {
                    location.resize(locLen);
                    if (!a_intfc->ReadRecordData(location.data(), locLen)) {
                        logger::error("Failed to read location data {} from v2 cosave", i);
                        return;
                    }
                }

                // Resolve FormID
                RE::FormID newFormID = 0;
                if (a_intfc->ResolveFormID(oldFormID, newFormID)) {
                    ContainerInfo info;
                    info.formID = newFormID;
                    if (stateVal > static_cast<std::uint8_t>(ContainerState::kGlobal)) {
                        logger::warn("Invalid state {} for container {:08X}, treating as off", stateVal, newFormID);
                        info.state = ContainerState::kOff;
                    } else {
                        info.state = static_cast<ContainerState>(stateVal);
                    }
                    info.name = std::move(name);
                    info.location = std::move(location);
                    m_playerOverrides[newFormID] = std::move(info);
                } else {
                    logger::debug("Failed to resolve v2 container {:08X}", oldFormID);
                }
            }

            logger::info("Loaded {} container overrides (cosave v2)", m_playerOverrides.size());

        } else {
            // Unknown future version - skip gracefully
            logger::warn("Cosave version {} is newer than supported (v{}). Skipping to avoid data corruption.",
                a_version, kCosaveVersion);
            // Don't try to read - we don't know the format
        }
    }

    void ContainerRegistry::Revert([[maybe_unused]] SKSE::SerializationInterface* a_intfc) {
        ClearPlayerOverrides();
    }
}
