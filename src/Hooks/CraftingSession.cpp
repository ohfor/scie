#include "Hooks/CraftingSession.h"

namespace Hooks {
    CraftingSessionManager* CraftingSessionManager::GetSingleton() {
        static CraftingSessionManager singleton;
        return &singleton;
    }

    void CraftingSessionManager::OnCraftingMenuOpen(RE::TESObjectREFR* a_furniture) {
        // Session initialization is now handled lazily in InventoryHooks
        // when GetOccupiedFurniture() is detected during inventory queries.
        // This event fires AFTER the initial inventory queries, so we can't
        // use it for initialization.
        (void)a_furniture;  // Unused
        logger::debug("CraftingMenu opened (session init handled by hooks)");
    }

    void CraftingSessionManager::OnCraftingMenuClose() {
        if (!g_craftingSession.active) {
            return;
        }

        logger::info("Crafting session ended (menu close)");
        g_craftingSession.Reset();
    }
}
