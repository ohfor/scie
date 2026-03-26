# SCIE Public API Reference

This document describes the public API for Skyrim Crafting Inventory Extender (SCIE), allowing external mod authors to query SCIE's container registry and merged inventory.

## Overview

SCIE exposes its functionality through:
1. **Papyrus Native Functions** - For script-based mods
2. **SKSE Messaging** - For native plugin authors (advanced)

The API provides access to:
- Registered container discovery
- Container state queries
- Merged inventory data (items from player + all containers)
- Session state information

## Papyrus API

### Script: `SCIE_API`

All API functions are global native functions on the `SCIE_API` script.

---

### Container Discovery

#### `GetRegisteredContainers`

```papyrus
ObjectReference[] Function GetRegisteredContainers(bool abGlobalOnly = true) global native
```

Returns an array of containers registered with SCIE.

**Parameters:**
- `abGlobalOnly` - If `true` (default), only returns global containers. This set is stable regardless of player location — the same containers are returned no matter which cell is loaded. If `false`, also includes local containers that happen to be loaded (cell-dependent, may vary between calls).

When `abGlobalOnly = true` (the default), the result includes:
- Player-promoted global containers
- Global containers from supported mods (LOTD, General Stores, Hip Bag)
- INI-configured global containers

**Returns:** Array of `ObjectReference` for matching containers.

**Example:**
```papyrus
; Get stable global set (recommended for binding/filtering)
ObjectReference[] globals = SCIE_API.GetRegisteredContainers()
Debug.Trace("SCIE has " + globals.Length + " global containers")

; Get everything including cell-dependent locals
ObjectReference[] all = SCIE_API.GetRegisteredContainers(false)
```

---

#### `GetContainerState`

```papyrus
int Function GetContainerState(ObjectReference akContainer) global native
```

Gets the state of a specific container.

**Parameters:**
- `akContainer` - The container to query

**Returns:**
- `0` = Off (disabled)
- `1` = Local (nearby access only)
- `2` = Global (accessible from anywhere)

**Example:**
```papyrus
ObjectReference chest = ; ... get a container reference
int state = SCIE_API.GetContainerState(chest)
if state == 2
    Debug.Trace("This container is globally accessible")
endif
```

---

### Inventory Queries

#### `GetCombinedItemCount`

```papyrus
int Function GetCombinedItemCount(Form akItem, int aiStationType = -1) global native
```

Gets the total count of an item from player inventory plus all active containers.

**Parameters:**
- `akItem` - The item form to count
- `aiStationType` - Station type filter (optional):
  - `-1` = No filter (all stations, default)
  - `0` = Crafting (forge, smelter, tanning, cooking)
  - `1` = Tempering (grindstone, workbench)
  - `2` = Enchanting
  - `3` = Alchemy

**Returns:** Total count of the item across all sources.

**Example:**
```papyrus
Form ironIngot = Game.GetForm(0x0005ACE4)  ; Iron Ingot
int total = SCIE_API.GetCombinedItemCount(ironIngot)
Debug.Trace("You have " + total + " iron ingots available for crafting")
```

---

#### `GetAvailableItems`

```papyrus
Form[] Function GetAvailableItems(int aiStationType = -1) global native
```

Gets all items available in the merged inventory.

**Parameters:**
- `aiStationType` - Station type filter (see `GetCombinedItemCount`)

**Returns:** Array of item forms available for crafting.

**Note:** Use with `GetAvailableItemCounts()` to get corresponding counts.

---

#### `GetAvailableItemCounts`

```papyrus
int[] Function GetAvailableItemCounts(int aiStationType = -1) global native
```

Gets counts for all available items. Returns a parallel array matching `GetAvailableItems()`.

**Parameters:**
- `aiStationType` - Station type filter (see `GetCombinedItemCount`)

**Returns:** Array of counts, same order as `GetAvailableItems()`.

**Example:**
```papyrus
Form[] items = SCIE_API.GetAvailableItems()
int[] counts = SCIE_API.GetAvailableItemCounts()

int i = 0
while i < items.Length
    Debug.Trace(items[i].GetName() + " x" + counts[i])
    i += 1
endwhile
```

---

#### `RefreshInventoryCache`

```papyrus
Function RefreshInventoryCache() global native
```

Forces SCIE to refresh its inventory cache. Call this if you've modified container contents programmatically and need updated counts immediately.

**Note:** The cache automatically refreshes when:
- Player opens a crafting station
- Items are crafted
- The API detects no active session

---

#### `IsSessionActive`

```papyrus
bool Function IsSessionActive() global native
```

Checks if the player is currently at a crafting station with SCIE active.

**Returns:** `true` if a crafting session is active.

**Example:**
```papyrus
if SCIE_API.IsSessionActive()
    Debug.Trace("Player is currently crafting")
endif
```

---

### Version Info

#### `GetAPIVersion`

```papyrus
int Function GetAPIVersion() global native
```

Gets the SCIE API version number.

**Returns:** Version as `major * 100 + minor` (e.g., `254` = v2.5.4)

**Example:**
```papyrus
int version = SCIE_API.GetAPIVersion()
if version >= 254
    ; Use v2.5.4+ features
endif
```

---

## Station Types

When filtering by station type, use these values:

| Value | Name | Stations |
|-------|------|----------|
| `-1` | All | No filter |
| `0` | Crafting | Forge, Smelter, Tanning Rack, Cooking Pot, Staff Enchanter, Carpenter's Workbench |
| `1` | Tempering | Grindstone, Armor Workbench |
| `2` | Enchanting | Arcane Enchanter |
| `3` | Alchemy | Alchemy Lab |

---

## Cache Behavior

SCIE maintains an inventory cache for performance. Understanding when to call `RefreshInventoryCache()`:

**Automatic refresh:**
- When player opens any crafting station
- After each craft operation
- When session ends (menu closes)

**Manual refresh needed:**
- After adding/removing items from containers via script
- After enabling/disabling containers via script
- When querying outside of a crafting session and containers have changed

**Performance tip:** Avoid calling `RefreshInventoryCache()` repeatedly in loops. Cache the results of `GetAvailableItems()` if you need to iterate multiple times.

---

## VR Limitations

On Skyrim VR, global containers may fail to resolve. This is a **Skyrim VR engine limitation**, not a SCIE limitation.

### Why This Happens

Skyrim manages placed objects (containers, furniture, NPCs) as "references" that exist within cells. When you're near a cell, its references are loaded into memory. When you leave, they're unloaded to save memory.

**Persistent vs Non-Persistent References:**

- **Non-persistent** references only exist in memory when their cell is loaded. Once unloaded, `LookupByID()` cannot find them.
- **Persistent** references (flag 0x400 in Creation Kit) maintain a handle even when their cell is unloaded, allowing lookup from anywhere.

**The VR Difference:**

Skyrim SE/AE maintains a global form table that allows `LookupByID()` to find references even in unloaded cells. Skyrim VR, built on an older engine branch optimized for the memory constraints of early VR hardware, does not maintain this table for non-persistent references.

This means SCIE's global container feature — which uses `LookupByID()` to access containers from anywhere — only works on VR if those containers are persistent.

### What Works on VR

| Container Type | VR Support |
|----------------|------------|
| **Local containers** (nearby) | Full support — scanned via cell iteration |
| **LOTD Safehouse** | Works — containers are persistent |
| **Elianora homes** (Eli's Breezehome, Routa, etc.) | Works — all refs are persistent |
| **Vanilla homes** (Breezehome, Honeyside, etc.) | **Does not work** as global — refs are not persistent |
| **CC content** (Tundra Homestead, etc.) | Mixed — some refs persistent, some not |
| **Player-marked globals** | Depends on whether the original ref is persistent |

### Recommendations for VR Users

1. **Use local containers** — always works, no persistence requirement
2. **Use LOTD Safehouse** — all containers are persistent
3. **Use quality mod homes** — authors like Elianora set refs as persistent
4. **Avoid promoting vanilla home containers to global** — they're not persistent
5. When a global fails, SCIE shows: "VR Limitation: Cannot access global container..."

### For Mod Authors

If you want your player home to support VR global containers, mark your container references as **Persistent** in the Creation Kit (right-click reference → Edit → check "Persistent Reference").

---

## SKSE Messaging (Native Plugins)

For native SKSE plugin authors, SCIE provides a messaging interface for C++ integration.

### Registration

Register as a listener for SCIE messages during your plugin load:

```cpp
SKSE::GetMessagingInterface()->RegisterListener("CraftingInventoryExtender", MyMessageHandler);
```

### Message Types

```cpp
namespace SCIE::API {
    enum class MessageType : std::uint32_t {
        // Requests (send TO SCIE)
        kRequestContainers      = 'SCRC',  // Request registered container list (globals only)
        kRequestContainerState  = 'SCRS',  // Request state of specific container
        kRequestInventory       = 'SCRI',  // Request merged inventory data

        // Responses (receive FROM SCIE)
        kResponseContainers     = 'SCPC',  // Container FormID array
        kResponseContainerState = 'SCPS',  // Container state response
        kResponseInventory      = 'SCPI'   // Inventory data response
    };
}
```

**Note:** `kRequestContainers` returns only global containers — the set is stable regardless of player location. Local containers are excluded because they depend on cell load state and would produce inconsistent results across calls.

### Request/Response Structures

```cpp
// Container state request
struct ContainerStateRequest {
    RE::FormID containerFormID;
};

// Container state response
struct ContainerStateResponse {
    RE::FormID containerFormID;
    std::int32_t state;  // 0=off, 1=local, 2=global
    bool found;          // true if container was in registry
};

// Inventory request
struct InventoryRequest {
    std::int32_t stationType;  // -1=all, 0=Crafting, 1=Tempering, 2=Enchanting, 3=Alchemy
};

// Inventory response
struct InventoryResponse {
    std::int32_t itemCount;         // Number of unique item types
    std::int32_t activeStationType; // -1 = no session, 0-3 = station type
};
```

### Example: Query Container State

```cpp
void QueryContainerState(RE::FormID containerID) {
    SCIE::API::ContainerStateRequest request;
    request.containerFormID = containerID;

    auto* messaging = SKSE::GetMessagingInterface();
    messaging->Dispatch(
        static_cast<uint32_t>(SCIE::API::MessageType::kRequestContainerState),
        &request,
        sizeof(request),
        "CraftingInventoryExtender"
    );
}

void MyMessageHandler(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == static_cast<uint32_t>(SCIE::API::MessageType::kResponseContainerState)) {
        auto* response = static_cast<SCIE::API::ContainerStateResponse*>(msg->data);
        if (response->found) {
            // state: 0=off, 1=local, 2=global
            logger::info("Container {:08X} state: {}", response->containerFormID, response->state);
        }
    }
}
```

### Direct API Access

For simpler integration, you can also link against SCIE and call the API directly:

```cpp
#include "API/APIMessaging.h"
#include "Services/APIService.h"

// Get all registered containers
auto containers = API::APIMessaging::GetSingleton()->GetRegisteredContainers();

// Query merged inventory
auto* api = Services::APIService::GetSingleton();
int ironCount = api->GetCombinedItemCount(ironIngotForm, -1);  // -1 = no station filter
```

Note: Direct linking requires building against SCIE's headers and is only recommended for tightly-coupled integrations.

---

## Examples

### Check if player has enough materials

```papyrus
Scriptname MyRecipeChecker extends Quest

Function CanCraftIronDagger()
    Form ironIngot = Game.GetForm(0x0005ACE4)
    Form leatherStrips = Game.GetForm(0x000800E4)

    int ingots = SCIE_API.GetCombinedItemCount(ironIngot, 0)  ; Crafting stations
    int strips = SCIE_API.GetCombinedItemCount(leatherStrips, 0)

    if ingots >= 1 && strips >= 1
        Debug.Notification("You can craft an iron dagger!")
    else
        Debug.Notification("Not enough materials")
    endif
EndFunction
```

### List all crafting containers

```papyrus
Scriptname MyContainerLister extends Quest

Function ListContainers()
    ObjectReference[] containers = SCIE_API.GetRegisteredContainers()

    Debug.Trace("=== SCIE Containers ===")
    int i = 0
    while i < containers.Length
        ObjectReference c = containers[i]
        int state = SCIE_API.GetContainerState(c)
        string stateName = "Off"
        if state == 1
            stateName = "Local"
        elseif state == 2
            stateName = "Global"
        endif
        Debug.Trace(c.GetDisplayName() + " [" + stateName + "]")
        i += 1
    endwhile
EndFunction
```

### Display available materials at forge

```papyrus
Scriptname MyForgeHelper extends Quest

Function ShowForgeMaterials()
    Form[] items = SCIE_API.GetAvailableItems(0)  ; Station type 0 = Crafting
    int[] counts = SCIE_API.GetAvailableItemCounts(0)

    Debug.Notification("Materials available: " + items.Length + " types")

    ; Show first 5 items
    int i = 0
    while i < items.Length && i < 5
        Debug.Trace(items[i].GetName() + " x" + counts[i])
        i += 1
    endwhile
EndFunction
```

---

## Version History

| Version | Changes |
|---------|---------|
| 2.5.10+ | `GetRegisteredContainers` defaults to globals-only; `abGlobalOnly` parameter added |
| 2.5.4 | Initial public API release |
