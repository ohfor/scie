#pragma once

namespace Hooks {
    // Forward declarations for use by SourceScanner
    namespace InventoryHooks {
    /// Initialize MinHook and install all inventory hooks
    /// Must be called during plugin load (before game data is loaded)
    /// Returns true if all hooks were installed successfully
    bool Install();

    /// Uninitialize MinHook (called on plugin unload, if any)
    void Uninstall();

    /// Function signatures for the hooked functions
    using GetContainerItemCount_t = std::int32_t(*)(RE::TESObjectREFR*, bool, bool);
    using GetInventoryItemEntryAtIdx_t = RE::InventoryEntryData*(*)(RE::TESObjectREFR*, std::int32_t, bool);
    using GetInventoryItemCount_t = std::int32_t(*)(RE::InventoryChanges*, RE::TESBoundObject*, void*);
    // RemoveItem uses struct return ABI - hidden result pointer as second param
    using RemoveItem_t = RE::ObjectRefHandle*(*)(RE::TESObjectREFR*, RE::ObjectRefHandle*,
        RE::TESBoundObject*, std::int32_t, RE::ITEM_REMOVE_REASON, RE::ExtraDataList*,
        RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);

    /// Get the original function pointers (for use by CraftingSession and SourceScanner)
    GetContainerItemCount_t GetOriginalGetContainerItemCount();
    GetInventoryItemEntryAtIdx_t GetOriginalGetInventoryItemEntryAtIdx();
    GetInventoryItemCount_t GetOriginalGetInventoryItemCount();
    }  // namespace InventoryHooks
}  // namespace Hooks
