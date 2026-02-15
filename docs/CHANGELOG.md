# Changelog

All notable user-facing changes to SCIE.

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
