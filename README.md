# SCIE - Skyrim Crafting Inventory Extender

An SKSE plugin that lets you craft using materials from nearby containers, not just your inventory.

## Features

- **Mark any container** with a lesser power - it becomes a crafting source
- **Global containers** - promote containers to work from anywhere, not just nearby
- **Follower inventory** - nearby followers and spouses contribute materials automatically
- **Horse saddlebags** - toggle your horse to use its inventory at crafting stations
- **Zero item movement** - items stay in containers, no transfer lag or risk of loss
- **MCM configuration** - filtering, scan distance, container management
- **Version independent** - single DLL works on SE, AE, and VR

Works with all crafting stations: forge, smelter, tanning rack, workbench, grindstone, alchemy lab, enchanting table, staff enchanter, cooking pot, and Hearthfire carpenter's workbench.

## How It Works

1. Two lesser powers are added on game load:
   - **SCIE: Toggle Container** - aim at a container and cast to mark it
   - **SCIE: Detect Containers** - highlights all nearby marked containers
2. Mark containers with the toggle power (green = local, gold = global, red = off)
3. Use any crafting station - items from marked containers appear in the menu
4. Materials are consumed directly from their source containers

## Requirements

- Skyrim SE, AE, or VR
- SKSE64 (or SKSE VR)
- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- SkyUI (optional, for MCM)

VR users also need [Skyrim VR ESL Support](https://www.nexusmods.com/skyrimspecialedition/mods/106712).

## Installation

Install via mod manager. The download includes pre-configured support for:
- LOTD Safehouse, General Stores, Hip Bag (global containers)
- All vanilla homes and Hearthfire properties
- 25+ popular player home mods

## Compatibility

**Works with:** LOTD, General Stores, Convenient Horses, Nether's Follower Framework, Essential Favorites, and any crafting overhaul.

**Incompatible:** Linked Crafting Storage (both mods intercept crafting - choose one).

## For Mod Authors

Pre-configure containers for your player home with INI files. See `ExampleModConfig.ini` in the download.

## Links

- [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/170497)
- [Changelog](docs/CHANGELOG.md)
- [Build Guide](docs/Build.md)

## Credits

- Inspired by [DavidJCobb's LE plugin](https://github.com/DavidJCobb/skyrim-classic-crafting-containers)
- Zero-transfer architecture inspired by [JerryYOJ](https://github.com/JerryYOJ/CraftingPullFromContainer-SKSE)
- Built with [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)

## License

MIT
