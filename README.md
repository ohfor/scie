# SCIE - Skyrim Crafting Inventory Extender

*Craft from your storage, not your pockets.*

An SKSE plugin that lets you craft using materials from nearby containers, follower inventories, and horse saddlebags. Items never move - SCIE intercepts crafting queries to present a combined inventory view.

<p align="center">
  <a href="https://patreon.com/ohfor"><img src="https://img.shields.io/badge/Patreon-Support-f96854?logo=patreon&logoColor=white" alt="Support on Patreon"></a>
</p>
<p align="center"><i>Want more mods? Help release me from my corporate prison. All support helps.</i></p>

## Features

- **Works everywhere** - Forge, smelter, tanning rack, workbench, grindstone, alchemy lab, enchanting table, staff enchanter, cooking pot, Hearthfire carpenter's workbench
- **Zero-transfer design** - Items stay in containers; no transfer lag or risk of loss
- **Global containers** - Promote any container to work from anywhere, or use built-in support for LOTD Safehouse, General Stores, and Hip Bag
- **Follower & spouse inventory** - Nearby followers automatically contribute materials
- **Horse saddlebags** - Toggle your horse to use saddlebag contents at crafting stations
- **MCM Containers page** - View and manage all marked containers grouped by location
- **MCM Filtering** - Control which item types are pulled per station type
- **Unsafe container warnings** - Respawning containers flagged to protect your items
- **Blazing fast** - 130x faster menu opening thanks to inventory caching
- **Version independent** - Single DLL works on SE, AE, and VR
- **Public API** - Query merged inventory from Papyrus scripts or native C++/SKSE plugins

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

**VR users** also need [VR Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/58101) and [Skyrim VR ESL Support](https://www.nexusmods.com/skyrimspecialedition/mods/106712).

**VR Limitation:** Global containers may not work for non-persistent references (engine limitation). SCIE will notify you when this happens. Local containers work normally.

## Installation

Install via mod manager. Downloads available on [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/170497):

**Main file includes:**
- Core SKSE plugin
- ESP with powers and MCM
- Recipe visibility patch
- Vanilla homes configuration

**Optional files:**
- **SCIE-HomeConfigs** - Pre-made configs for 25+ player home mods, plus LOTD/General Stores/Hip Bag
- **SCIE-CCOR-Patch** - Fixes smelter recipes for Complete Crafting Overhaul Remastered
- **SCIE-DLC-Patch** - Fixes Dawnguard crossbow bolts and Forgotten Seasons recipes
- **SCIE-Babel** - AI-generated translations for 12 languages

## Compatibility

**Works with:** LOTD, General Stores, Convenient Horses, Simplest Horses, Nether's Follower Framework, Essential Favorites, CACO, CCOR, and most crafting overhauls.

**Incompatible:** Linked Crafting Storage (both mods intercept crafting - choose one).

## For Mod Authors

### Public API

Query SCIE's merged inventory from your own mods:

**Papyrus:**
```papyrus
; Get total iron ingots available across all SCIE sources
int ironCount = SCIE_API.GetCombinedItemCount(IronIngot)

; Get all available crafting materials
Form[] items = SCIE_API.GetAvailableItems()
int[] counts = SCIE_API.GetAvailableItemCounts()

; Check which containers SCIE is using
ObjectReference[] containers = SCIE_API.GetRegisteredContainers()
```

**C++/SKSE:**
```cpp
#include "Services/APIService.h"

auto* api = Services::APIService::GetSingleton();
int32_t count = api->GetCombinedItemCount(ironIngot);

std::vector<RE::TESBoundObject*> items;
std::vector<int32_t> counts;
api->GetAvailableItems(-1, items, counts);
```

Full documentation: [docs/API.md](docs/API.md)

### Pre-configure Containers

Create INI files in `Data/SKSE/Plugins/CraftingInventoryExtender/`:

```ini
[Containers]
YourMod.esp|0x123ABC = true  ; Local container

[GlobalContainers]
YourMod.esp|0x456DEF = true  ; Global container
```

See `ExampleModConfig.ini` in the download for detailed examples.

## Building

See [docs/Build.md](docs/Build.md) for build instructions.

## Links

- [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/170497)
- [Changelog](docs/CHANGELOG.md)
- [API Documentation](docs/API.md)

## Credits

- Inspired by [DavidJCobb's LE plugin](https://github.com/DavidJCobb/skyrim-classic-crafting-containers)
- Zero-transfer architecture inspired by [JerryYOJ](https://github.com/JerryYOJ/CraftingPullFromContainer-SKSE)
- Built with [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)

## License

MIT
