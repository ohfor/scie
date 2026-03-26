Scriptname SCIE_API Hidden
{Public API for Skyrim Crafting Inventory Extender (SCIE)

Exposes SCIE's container registry and merged inventory to external mod authors.
All functions are native (implemented in C++) for maximum performance.

Station Types:
  -1 = No filter (all stations)
   0 = Crafting (forge, smelter, tanning, cooking, etc.)
   1 = Tempering (grindstone, workbench)
   2 = Enchanting
   3 = Alchemy

Container States:
   0 = Off (disabled)
   1 = Local (nearby access only)
   2 = Global (accessible from anywhere)}


; ============================================================================
; CONTAINER DISCOVERY
; ============================================================================

; Get registered containers known to SCIE
; abGlobalOnly: if true (default), only returns global containers — the set is
;   stable regardless of player location. If false, also includes local
;   containers that happen to be loaded (cell-dependent, may vary between calls).
ObjectReference[] Function GetRegisteredContainers(bool abGlobalOnly = true) global native

; Get the state of a specific container
; Returns: 0 = off, 1 = local, 2 = global
int Function GetContainerState(ObjectReference akContainer) global native


; ============================================================================
; INVENTORY QUERIES
; ============================================================================

; Get combined item count from player + all active containers
; aiStationType: -1 for no filter, or 0-3 for specific station type
; Returns total count of akItem across all sources
int Function GetCombinedItemCount(Form akItem, int aiStationType = -1) global native

; Get all available items from merged inventory
; aiStationType: -1 for no filter, or 0-3 for specific station type
; Returns array of Forms (items available for crafting)
Form[] Function GetAvailableItems(int aiStationType = -1) global native

; Get counts for all available items (parallel array with GetAvailableItems)
; Call after GetAvailableItems to get corresponding counts
; aiStationType: -1 for no filter, or 0-3 for specific station type
int[] Function GetAvailableItemCounts(int aiStationType = -1) global native

; Force refresh of the inventory cache
; Call this if you've modified container contents and need updated counts
Function RefreshInventoryCache() global native

; Check if a crafting session is currently active
; Returns true if player is at a crafting station with SCIE active
bool Function IsSessionActive() global native


; ============================================================================
; VERSION INFO
; ============================================================================

; Get SCIE API version (format: major * 100 + minor, e.g., 254 = v2.5.4)
int Function GetAPIVersion() global native
