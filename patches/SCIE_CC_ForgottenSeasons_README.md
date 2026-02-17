# SCIE CC Forgotten Seasons Compatibility Patch

Compatibility patch for **Crafting Inventory Extender (SCIE)** with Creation Club: Forgotten Seasons.

## Requirements

- Crafting Inventory Extender (SCIE)
- CC: Forgotten Seasons (cctwbsse001-puzzledungeon.esm)

## Installation

Install after SCIE. This patch has no load order requirements beyond loading after CraftingInventoryExtender.esp.

## What This Patch Does

The Forgotten Seasons creation adds recipes with `GetItemCount` conditions that check if you have materials in your player inventory. These conditions are evaluated **before** the crafting menu populates, hiding recipes entirely when materials are in SCIE containers instead of your inventory.

This patch adds SCIE-aware duplicate recipes that appear when:
- SCIE is active (you're at a crafting station with SCIE containers)
- Materials are NOT in your player inventory (they're in SCIE containers)

The original recipes still work normally when materials are in your inventory.

## Patch Statistics

| Source | Recipes |
|--------|--------:|
| CC: Forgotten Seasons | 9 |

### Forgotten Seasons Recipes (9)

Seasonal Crown crafting and transformation recipes from the Puzzle Dungeon creation.

## Technical Details

- **Plugin type**: ESL-flagged (does not use a plugin slot)
- **Records**: 9 COBJ (Constructible Object)
- **Masters**: Skyrim.esm, CraftingInventoryExtender.esp, cctwbsse001-puzzledungeon.esm

## DLC Content

For official DLC compatibility (Dawnguard, Hearthfire), see the separate **SCIE-DLC** patch.
