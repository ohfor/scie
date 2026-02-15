# SCIE DLC/CC Compatibility Patch

Compatibility patch for **Crafting Inventory Extender (SCIE)** with official DLC and Creation Club content.

## Requirements

- Crafting Inventory Extender (SCIE)
- Dawnguard DLC
- Hearthfire DLC
- CC: Forgotten Seasons (cctwbsse001-puzzledungeon.esm)

## Installation

Install after SCIE. This patch has no load order requirements beyond loading after CraftingInventoryExtender.esp.

## What This Patch Does

Some official DLC and Creation Club recipes use `GetItemCount` conditions that check if you have materials in your player inventory. These conditions are evaluated **before** the crafting menu populates, hiding recipes entirely when materials are in SCIE containers instead of your inventory.

This patch adds SCIE-aware duplicate recipes that appear when:
- SCIE is active (you're at a crafting station with SCIE containers)
- Materials are NOT in your player inventory (they're in SCIE containers)

The original recipes still work normally when materials are in your inventory.

## Patch Statistics

| Source | Recipes |
|--------|--------:|
| Dawnguard | 2 |
| Hearthfire | 8 |
| CC: Forgotten Seasons | 9 |
| **Total** | **19** |

### Dawnguard Recipes (2)

- Vale Deer Hide to Leather
- Vale Sabre Cat Hide to Leather

### Hearthfire Recipes (8)

House building door recipes that check for construction materials.

### CC: Forgotten Seasons Recipes (9)

Seasonal Crown crafting and transformation recipes from the Puzzle Dungeon creation.

## Technical Details

- **Plugin type**: ESL-flagged (does not use a plugin slot)
- **Records**: 19 COBJ (Constructible Object)
- **Masters**: Skyrim.esm, CraftingInventoryExtender.esp, Dawnguard.esm, HearthFires.esm, cctwbsse001-puzzledungeon.esm

## Note on Hearthfire

Most Hearthfire house building works fine with SCIE without this patch. The house building system uses `GetItemCount == X` conditions (checking build stage progress), not `GetItemCount >= 1` (checking for materials). Only 8 specific door recipes needed patching.
