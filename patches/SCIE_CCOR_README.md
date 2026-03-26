# SCIE CCOR Compatibility Stub

Save-compatibility stub for **Crafting Inventory Extender (SCIE)** and **Complete Crafting Overhaul Remastered (CCOR)**.

## What Is This?

This ESP is an **empty stub** (0 records). It exists solely to prevent "missing plugin" errors when loading saves that previously used the SCIE CCOR recipe patch.

As of SCIE v2.5.8, recipe visibility is handled natively by the core plugin. The duplicate COBJ records that were previously in this file are no longer needed.

## Do I Need This?

- **Existing saves** that had the old CCOR patch installed: Yes, keep this installed to avoid save load errors.
- **New saves** or saves that never used the CCOR patch: No, this file is not needed.

## Technical Details

- **Plugin type**: ESL-flagged (does not use a plugin slot)
- **Records**: 0 (empty stub)
- **Masters**: Skyrim.esm, CraftingInventoryExtender.esp, Complete Crafting Overhaul_Remastered.esp
