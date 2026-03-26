# Changelog

All notable user-facing changes to SCIE.

## [2.6.0] - 2026-03-26

### Added
- **MCM Containers page: INI-configured containers display**: Below player-marked containers, a new read-only section shows all containers from INI config files. Containers are grouped by source file and split into "Available" (in-range) and "Out of Range" sub-sections. Useful for seeing which pre-configured containers SCIE knows about and whether they're reachable.
- **HomeConfigs: Waking Nightmare** (SlimeSire): 5 containers — Stockpile, Examination Table, Occult Cache, Cupboard, Sleepers' Quarters
- **HomeConfigs: King of Worms | Queen of Dawn** (SlimeSire): 4 containers — Vaunted Relics (Chantry), Spectral Armoury + 2 Sload Eggsacs (Plague Pit)

### Fixed
- **SLID integration: inconsistent container count across cell loads**: External mods querying SCIE's container list via the API could receive different results depending on which cell the player was in. Now returns a stable set (global containers only) by default.

---

## [2.5.9] - 2026-03-09

### Fixed
- **VR: Material counts showing 0 at crafting stations**: When selecting a recipe, the material list at the bottom showed "0" for all ingredients despite SCIE knowing the correct counts. Root cause: SCIE's inventory count hook was mapped to the wrong VR function address, leaving the function that populates the material display completely unhooked. Added a VR-only hook (Hook 3b) targeting the correct address. SE/AE are completely unaffected — the fix only activates on VR.
- **VR: Crafting plays sound but doesn't produce items**: The craft handler checks material availability before consuming — since it used the same unhooked function, it found 0 materials and aborted silently. Fixed by the same Hook 3b above.
- **VR: Hook 5 crash on startup** (hotfix from v2.5.8.1): Hook 5 used `REL::RelocationID` which doesn't support VR, causing a crash on game launch. Changed to `REL::VariantID` with the correct VR offset.

---

## [2.5.8] - 2026-03-08

### Changed
- **Recipe visibility patches replaced with stubs**: The four optional recipe patch ESPs (`CraftingInventoryExtenderVanillaCountFix.esp`, `SCIE_DLC_RecipesPatch.esp`, `SCIE_CC_ForgottenSeasons_RecipesPatch.esp`, `SCIE_CCOR_RecipesPatch.esp`) are now empty stubs. Research confirmed SCIE's core inventory hook already makes recipes visible when materials are in SCIE containers — the duplicate recipes in the patch ESPs were unnecessary. **Existing users**: the stubs maintain save compatibility. You can safely leave them installed or remove them if you started a new save.

### Fixed
- **SLID network detection on game load**: SLID networks were not detected until the second game load because the network list was requested too early during save loading. Now requested after all plugins finish initializing.

---

## [2.5.7] - 2026-03-01

### Fixed
- **MCM showing raw format strings**: The MCM Containers page displayed `{0} (SCIE OFF)` instead of `[OFF]` due to duplicate translation keys. Fixed by giving notification strings unique key names. Also fixes the SCIE-Babel translations package.
- **VR powers not working**: Fixed incorrect form ID computation on VR when other ESL mods are installed. Powers now resolve correctly regardless of load order. **VR users upgrading from a previous version**: go to MCM > Maintenance > Revoke Powers, then Grant Powers once to clean up stale entries from your save.

---

## [2.5.6] - 2026-02-28

### Fixed
- **Intermittent crafting station crash**: Fixed a race condition where inventory enumeration could use stale index data after crafting, causing access violation crashes. Most likely to occur during extended play sessions with DescriptionFramework installed.
- **Powers equipping to hands**: Toggle and Detect powers now correctly assign to the Powers/Shout key instead of left/right hand
- **Non-persistent global containers**: Non-persistent containers (most CC homestead containers, some vanilla barrels) can no longer be promoted to global — the toggle power now skips GLOBAL and cycles directly to OFF with an explanation notification. These containers are evicted from memory when you leave the area, making global access impossible. Persistent containers (LOTD Safehouse, Hearthfire built homes, General Stores) are unaffected. Runtime notification also changed from misleading "VR Limitation" to clearer "{name} not loaded (too far away)"

---

## [2.5.5] - 2026-02-17

### Added
- **Khajiit Will Follow support**: KWF follower storage containers now contribute materials (same safety filter as NFF)
- **SLID integration**: Skyrim Linked Item Distribution can query SCIE containers via SKSE messaging API

### Changed
- **Split DLC/CC recipe patches**: `SCIE_DLC_RecipesPatch.esp` (Dawnguard + Hearthfire) and `SCIE_CC_ForgottenSeasons_RecipesPatch.esp` (Creation Club) are now separate files for cleaner load order management
- **BREAKING (native plugins only)**: `InventoryResponse.sessionActive` (bool) replaced with `activeStationType` (int32_t: -1 = no session, 0-3 = station type)

### Fixed
- **SKSE messaging API**: Now dispatches responses correctly (was stub). Native plugins can query containers via SKSE messaging.

---

## [2.5.4] - 2026-02-15

### Added
- **Public API for mod authors**: New `SCIE_API` script exposes container registry and merged inventory queries to external mods. See [API.md](API.md) for full documentation.
- **VR limitation notification**: Shows on-screen warning when a global container cannot be accessed on VR (non-persistent references)
- **3-tier logging**: New Info/Debug/Trace log levels in MCM (replaces binary debug toggle)

### Fixed
- **VR recipe availability**: Recipes now correctly show as available when materials are in SCIE containers on VR
- Improved stability with additional defensive checks in inventory hooks

---

## [2.5.3] - 2026-02-01

### Added
- **SCIE-Babel translation pack**: AI-generated translations for 12 languages (French, German, Italian, Spanish, Russian, Polish, Czech, Turkish, Japanese, Korean, Chinese Simplified, Chinese Traditional). Optional download. Community corrections welcome.

### Fixed
- Items disappearing progressively at enchanting stations when crafting multiple items
- Better Bulk Enchanting compatibility regression from v2.5.2

---

## [2.5.2] - 2026-01-31

### Fixed
- Items split across multiple containers now show correct combined totals

---

## [2.5.1] - 2026-01-30

### Added
- **INI containers toggle**: Disable all INI-configured containers via MCM if you prefer marking everything yourself
- **Filter lockdown**: WEAP/ARMO filters locked off at tempering/enchanting (items must be in player inventory for those stations)
- **Unsafe container warning**: Respawning containers show "UNSAFE" warning
- **Full translation support**: All 205 strings externalized for community translation
- Community translations: Russian (SsergioA), Chinese Simplified (Lvtree)

### Fixed
- Crash on save reload after crafting
- INI override support: `=false` now correctly disables containers
- VR form lookups now working

---

## [2.5.0] - 2026-01-29

### Added
- **Global container promotion**: Promote any container to global access (works from anywhere)
- **MCM Containers page**: View and manage all player-marked containers by location
- Automatic save migration from older versions

### Fixed
- VR support improvements
- Various MCM and container state fixes

---

## [2.4.0] - 2026-01-28

### Added
- **Follower & spouse inventory**: Nearby followers/spouses contribute materials automatically
- **NFF Additional Inventory**: Nether's Follower Framework satchels supported
- **MCM Compatibility page**: Shows detected mods and SCIE's response
- **Convenient Horses support**: Horse saddlebags work with CH
- **Essential Favorites**: Favorited items excluded from crafting

### Fixed
- Cache refresh after crafting
- Toggle power feedback on global containers
- Toggle power no longer accepts invalid targets

---

## [2.3.0] - 2026-01-27

### Added
- **MCM Filtering**: Per-station item type filtering
- Plugin status detection in MCM
- Item deduplication across containers

### Fixed
- Recipe visibility patch included in distribution
- Double-counting bug with global containers
- Debug logging now flushes properly

---

## [2.2.1] - 2026-01-22

### Added
- Recipe visibility patch for vanilla tanning/tempering recipes

---

## [2.2.0] - 2026-01-20

### Added
- Horse saddlebag support

---

## [2.1.4] - 2026-01-18

### Added
- Skyrim VR support

---

## [2.1.0] - 2026-01-14

### Added
- Global containers toggle in MCM
- Mod compatibility detection (LOTD, General Stores)
- Hip Bag support

---

## [2.0.0] - 2026-01-12

### Changed
- Complete architecture rewrite: zero-transfer design
- Single DLL for all Skyrim versions

### Added
- Global containers (LOTD Safehouse, General Stores)
- MCM configuration
- Papyrus API
- Toggle and Detect powers
- Cosave persistence

---

## [1.0.0] - Initial Release

- Basic crafting from nearby containers
- INI-based container configuration
- Toggle power for marking containers
