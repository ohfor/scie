#include "Version.h"
#include "Hooks/InventoryHooks.h"
#include "Hooks/CraftingSession.h"
#include "Services/ContainerManager.h"
#include "Services/ContainerRegistry.h"
#include "Services/INISettings.h"
#include "Services/TranslationService.h"
#include "API/APIMessaging.h"
#include "Papyrus/PapyrusInterface.h"

#include <ShlObj.h>  // For SHGetKnownFolderPath, FOLDERID_Documents

// Plugin version info for SKSE
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v;
    v.PluginVersion(REL::Version(Version::MAJOR, Version::MINOR, Version::PATCH, 0));
    v.PluginName(Version::NAME);
    v.AuthorName(Version::AUTHOR);
    v.UsesAddressLibrary();  // We resolve addresses via Address Library
    v.UsesNoStructs();       // We don't depend on game struct layouts
    // No CompatibleVersions - we validate Address Library at runtime and handle failures gracefully
    return v;
}();

namespace {
    /// Get the correct SKSE log directory by deriving the game name from the DLL's own path.
    /// The DLL lives in {GameRoot}\Data\SKSE\Plugins\, so we can extract the game folder name.
    /// This works for SSE, VR, GOG, or any variant without hardcoding.
    std::optional<std::filesystem::path> GetLogDirectory() {
        // Get path to this DLL
        HMODULE hModule = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&GetLogDirectory), &hModule)) {
            return std::nullopt;
        }

        wchar_t dllPath[MAX_PATH];
        if (GetModuleFileNameW(hModule, dllPath, MAX_PATH) == 0) {
            return std::nullopt;
        }

        // DLL is at: {GameRoot}\Data\SKSE\Plugins\CraftingInventoryExtender.dll
        // Navigate up 4 levels to get game root, then extract folder name
        std::filesystem::path dllLocation = dllPath;
        auto gameRoot = dllLocation.parent_path()  // Plugins
                                   .parent_path()  // SKSE
                                   .parent_path()  // Data
                                   .parent_path(); // GameRoot (e.g., "Skyrim Special Edition")

        auto gameFolderName = gameRoot.filename();  // e.g., "Skyrim Special Edition"

        // Get Documents folder
        wchar_t* docPath = nullptr;
        if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docPath))) {
            return std::nullopt;
        }

        std::filesystem::path result = docPath;
        CoTaskMemFree(docPath);

        // Build: Documents\My Games\{GameFolderName}\SKSE
        result /= "My Games";
        result /= gameFolderName;
        result /= "SKSE";
        return result;
    }

    void InitializeLogging() {
        auto logDir = GetLogDirectory();
        if (!logDir) {
            SKSE::stl::report_and_fail("Could not determine Documents folder for logging");
        }

        // Ensure directory exists
        std::filesystem::create_directories(*logDir);

        auto path = *logDir / fmt::format(FMT_STRING("{}.log"), Version::NAME);

        try {
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
            auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
            log->set_level(spdlog::level::info);
            log->flush_on(spdlog::level::info);

            spdlog::set_default_logger(std::move(log));
            spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);

            logger::info("{} v{}.{}.{} loaded", Version::NAME, Version::MAJOR, Version::MINOR, Version::PATCH);

            auto ver = REL::Module::get().version();
            const char* runtime = "Unknown";
            if (REL::Module::IsVR()) {
                runtime = "VR";
            } else if (ver >= SKSE::RUNTIME_SSE_1_6_317) {
                runtime = "AE";
            } else {
                runtime = "SE";
            }
            logger::info("Game: Skyrim {} v{}.{}.{}.{}", runtime, ver[0], ver[1], ver[2], ver[3]);
        } catch (const std::exception& ex) {
            SKSE::stl::report_and_fail(fmt::format("Log init failed: {}", ex.what()));
        }
    }

    // Cached pointer to SCIE_CraftingActive GLOB for fast access
    RE::TESGlobal* g_craftingActiveGlobal = nullptr;

    /// Set the SCIE_CraftingActive GLOB value
    /// This controls visibility of SCIE-aware recipe duplicates in the optional patch ESP
    void SetCraftingActiveGlobal(bool active) {
        if (!g_craftingActiveGlobal) {
            // Lazy lookup - only done once
            auto* dh = RE::TESDataHandler::GetSingleton();
            g_craftingActiveGlobal = dh ? dh->LookupForm<RE::TESGlobal>(0x809, "CraftingInventoryExtender.esp") : nullptr;
            if (!g_craftingActiveGlobal) {
                // GLOB not found - this is expected if user hasn't updated their ESP yet
                // or if the recipe visibility patch isn't installed
                static bool s_warned = false;
                if (!s_warned) {
                    logger::debug("SCIE_CraftingActive GLOB not found - recipe visibility patch not active");
                    s_warned = true;
                }
                return;
            }
            logger::info("Found SCIE_CraftingActive GLOB (FormID {:08X})", g_craftingActiveGlobal->GetFormID());
        }

        float newValue = active ? 1.0f : 0.0f;
        if (g_craftingActiveGlobal->value != newValue) {
            g_craftingActiveGlobal->value = newValue;
            logger::debug("SCIE_CraftingActive set to {}", active ? 1 : 0);
        }
    }

    /// Check if a furniture reference is a crafting station
    bool IsCraftingFurniture(RE::TESObjectREFR* ref) {
        if (!ref) return false;

        auto* furn = ref->GetBaseObject()->As<RE::TESFurniture>();
        if (!furn) return false;

        // Check if it has any workbench type (crafting stations do)
        // This covers forge, tanning rack, smelter, cooking pot, alchemy, enchanting, etc.
        using BenchType = RE::TESFurniture::WorkBenchData::BenchType;
        return furn->workBenchData.benchType.get() != BenchType::kNone;
    }

    // Activation event handler - fires BEFORE crafting menu opens
    // This is where we set the GLOB so conditions are evaluated correctly
    class ActivationEventHandler : public RE::BSTEventSink<RE::TESActivateEvent> {
    public:
        static ActivationEventHandler* GetSingleton() {
            static ActivationEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESActivateEvent* a_event,
            RE::BSTEventSource<RE::TESActivateEvent>*) override
        {
            if (!a_event) return RE::BSEventNotifyControl::kContinue;

            // Only care about player activating things
            if (!a_event->actionRef || !a_event->actionRef->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* target = a_event->objectActivated.get();
            if (!target) return RE::BSEventNotifyControl::kContinue;

            // Check if this is crafting furniture
            if (IsCraftingFurniture(target)) {
                logger::debug("Crafting furniture activated: {}", target->GetName());

                // Set GLOB=1 BEFORE menu opens so conditions evaluate correctly
                // We set it unconditionally - if no SCIE containers exist, the recipes
                // still won't be craftable (material checks still apply)
                SetCraftingActiveGlobal(true);
            }

            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        ActivationEventHandler() = default;
    };

    // Menu event handler to detect crafting menu open/close
    class MenuEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static MenuEventHandler* GetSingleton() {
            static MenuEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!a_event) return RE::BSEventNotifyControl::kContinue;

            auto menuName = a_event->menuName;

            // Check for crafting-related menus
            if (menuName == RE::CraftingMenu::MENU_NAME) {
                auto* sessionMgr = Hooks::CraftingSessionManager::GetSingleton();

                if (a_event->opening) {
                    logger::info("CraftingMenu OPENED");
                    // Session may already be initialized by activation handler
                    // OnCraftingMenuOpen is safe to call multiple times
                    sessionMgr->OnCraftingMenuOpen(nullptr);
                } else {
                    logger::info("CraftingMenu CLOSED");
                    sessionMgr->OnCraftingMenuClose();

                    // Always reset GLOB to 0 on menu close
                    SetCraftingActiveGlobal(false);
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        MenuEventHandler() = default;
    };

    void RegisterEventHandlers() {
        // Register menu event handler
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->AddEventSink(MenuEventHandler::GetSingleton());
            logger::info("Menu event handler registered");
        } else {
            logger::error("Failed to get UI singleton for menu events");
        }

        // Register activation event handler (for setting GLOB before menu opens)
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink(ActivationEventHandler::GetSingleton());
            logger::info("Activation event handler registered");
        } else {
            logger::error("Failed to get ScriptEventSourceHolder for activation events");
        }
    }

    /// Grant SCIE powers to player if not already present
    void GrantPowersToPlayer() {
        if (!Services::INISettings::GetSingleton()->GetEnabled()) {
            logger::debug("Mod disabled, skipping power grant");
            return;
        }

        if (!Services::INISettings::GetSingleton()->GetAddPowersToPlayer()) {
            logger::debug("Power auto-add disabled, skipping");
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("Player not available for power grant");
            return;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::error("TESDataHandler not available");
            return;
        }

        constexpr const char* pluginName = "CraftingInventoryExtender.esp";

        auto* togglePower = dataHandler->LookupForm<RE::SpellItem>(0x802, pluginName);
        auto* detectPower = dataHandler->LookupForm<RE::SpellItem>(0x804, pluginName);

        int added = 0;

        if (togglePower) {
            if (!player->HasSpell(togglePower)) {
                player->AddSpell(togglePower);
                logger::info("Added SCIE_TogglePower to player");
                added++;
            }
        } else {
            logger::warn("SCIE_TogglePower not found in {}", pluginName);
        }

        if (detectPower) {
            if (!player->HasSpell(detectPower)) {
                player->AddSpell(detectPower);
                logger::info("Added SCIE_DetectPower to player");
                added++;
            }
        } else {
            logger::warn("SCIE_DetectPower not found in {}", pluginName);
        }

        if (added > 0) {
            logger::info("Granted {} SCIE power(s) to player", added);
        } else {
            logger::debug("Player already has all SCIE powers");
        }
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                logger::info("Data loaded, initializing...");
                Services::INISettings::GetSingleton()->Load();
                Services::TranslationService::GetSingleton()->Load();
                Services::ContainerRegistry::GetSingleton()->Initialize();
                Services::ContainerManager::GetSingleton()->Initialize();
                API::APIMessaging::GetSingleton()->Initialize();
                RegisterEventHandlers();

                // Register Papyrus native functions
                if (auto* papyrus = SKSE::GetPapyrusInterface()) {
                    papyrus->Register(Papyrus::RegisterFunctions);
                }
                break;
            case SKSE::MessagingInterface::kPostLoadGame:
                logger::info("Game loaded");
                GrantPowersToPlayer();
                break;
            case SKSE::MessagingInterface::kNewGame:
                logger::info("New game started");
                GrantPowersToPlayer();
                break;
        }
    }
}

// Legacy Query entry point for SE 1.5.97 (old SKSE64 looks for this)
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_info) {
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = Version::NAME.data();
    a_info->version = Version::MAJOR;
    return true;
}

// Main load entry point (SKSEPluginLoad macro expands to SKSEPlugin_Load)
SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    // SKSE::Init must come first - logging depends on SKSE::log::log_directory()
    SKSE::Init(a_skse);

    InitializeLogging();

    logger::info("{} is loading...", Version::NAME);

    // Register for SKSE messages
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        logger::error("Failed to register messaging listener");
        return false;
    }

    // Register for cosave serialization
    auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Services::ContainerRegistry::kCosaveID);
    serialization->SetSaveCallback(Services::ContainerRegistry::OnGameSaved);
    serialization->SetLoadCallback(Services::ContainerRegistry::OnGameLoaded);
    serialization->SetRevertCallback(Services::ContainerRegistry::OnRevert);
    logger::info("Cosave serialization registered");

    // Install inventory hooks (zero-transfer architecture)
    logger::info("Installing hooks...");

    if (!Hooks::InventoryHooks::Install()) {
        // Hook installation failed - user has already been notified via message box
        // Return true anyway so SKSE doesn't show a scary "fatal error" message
        // The mod is simply disabled for this session
        logger::critical("{} DISABLED - hook installation failed (see message box for details)", Version::NAME);
        logger::critical("The game will continue without SCIE functionality");
        return true;  // Don't fail SKSE load - just run without our features
    }

    logger::info("Hooks installed successfully");

    logger::info("{} loaded successfully", Version::NAME);
    return true;
}
