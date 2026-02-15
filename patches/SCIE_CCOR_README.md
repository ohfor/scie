# SCIE CCOR Compatibility Patch

Compatibility patch for **Crafting Inventory Extender (SCIE)** and **Complete Crafting Overhaul Remastered (CCOR)**.

## Requirements

- Crafting Inventory Extender (SCIE)
- Complete Crafting Overhaul Remastered

## Installation

Install after both SCIE and CCOR. Load order should be:

1. Complete Crafting Overhaul_Remastered.esp
2. CraftingInventoryExtender.esp
3. SCIE_CCOR_RecipesPatch.esp (this patch)

## What This Patch Does

CCOR adds `GetItemCount` conditions to many crafting recipes. For example:

```
RecipeIngotIron:
  Condition: GetItemCount(IronOre) >= 1
  Components: Iron Ore (5)
  Creates: Iron Ingot
```

This condition is evaluated **before** the crafting menu populates. If you don't have Iron Ore in your player inventory, the recipe is hidden entirely - SCIE's inventory hooks never get a chance to fire.

This patch adds SCIE-aware duplicate recipes:

```
SCIE_RecipeIngotIron:
  Condition: GetGlobalValue(SCIE_CraftingActive) >= 1
         AND GetItemCount(IronOre) < 1
  Components: Iron Ore (5)
  Creates: Iron Ingot
```

This recipe appears when:
- SCIE is active (you're at a crafting station with SCIE containers)
- Materials are NOT in your player inventory (they're in SCIE containers)

The original CCOR recipe still works normally when materials are in your inventory.

## Patch Statistics

| | Count |
|---|---:|
| **Total patched recipes** | 1,122 |
| CCOR total recipes | 2,353 |
| Recipes needing patch | 47.7% |

### Recipes by Crafting Station

| Station | Recipes | % |
|---|---:|---:|
| Smelter | 497 | 44.1% |
| Skyforge | 453 | 40.2% |
| Tanning Rack | 159 | 14.1% |
| Cooking (Hearthfire) | 4 | 0.4% |
| Other | 13 | 1.2% |

### Why So Many Smelter Recipes?

CCOR overhauls smelting comprehensively:
- All ore-to-ingot recipes (iron, steel, silver, gold, etc.)
- Dwemer scrap smelting
- Jewelry material processing

Every one of these has a `GetItemCount` condition to check if you have the ore/scrap.

### Why So Many Skyforge Recipes?

CCOR moves many smithing recipes from the regular forge to the Skyforge as a progression/balance feature. All moved recipes inherit CCOR's `GetItemCount` condition pattern.

## Technical Details

- **Plugin type**: ESL-flagged (does not use a plugin slot)
- **Records**: 1,126 COBJ (Constructible Object)
- **Masters**: Skyrim.esm, CraftingInventoryExtender.esp, Complete Crafting Overhaul_Remastered.esp

## Comparison to Vanilla Patch

SCIE's main mod includes `CraftingInventoryExtenderVanillaCountFix.esp` which patches 15 vanilla recipes (14 tanning, 1 tempering). Vanilla Skyrim uses `GetItemCount` conditions sparingly.

CCOR uses them extensively, hence this separate 1,126-recipe patch.

