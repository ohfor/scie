# Build Guide

How to build SCIE from source.

## Prerequisites

1. **Visual Studio 2022** with C++ desktop development workload
2. **CMake 3.21+** (included with VS2022)
3. **vcpkg** - C++ package manager
4. **Skyrim Creation Kit** - For Papyrus compiler (optional, only if modifying scripts)

### Setting Up vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

Set environment variable `VCPKG_ROOT=C:\vcpkg` (add to system environment variables).

## Building

### Configure (first time only)

```cmd
cmake --preset release
```

### Build

From a Visual Studio Developer Command Prompt:

```cmd
set VCPKG_ROOT=C:\vcpkg
cmake --build build\release --config Release
```

Output: `build\release\Release\CraftingInventoryExtender.dll`

### Install

Copy to your Skyrim installation:
- `CraftingInventoryExtender.dll` → `Data\SKSE\Plugins\`
- `Data\CraftingInventoryExtender.esp` → `Data\`
- `Data\SKSE\Plugins\CraftingInventoryExtender.ini` → `Data\SKSE\Plugins\`

## Papyrus Scripts

Pre-compiled scripts are included in `scripts/papyrus/compiled/`.

If you modify the `.psc` sources, recompile using the Creation Kit's Papyrus compiler. The MCM script requires SkyUI SDK sources.

### SkyUI SDK Setup

SCIE_MCM.psc extends SkyUI's `SKI_ConfigBase`. Download SDK sources from [SkyUI GitHub](https://github.com/schlangster/skyui/tree/master/dist/Data/Scripts/Source) and place in your `Data\Scripts\Source\` folder.

## Dependencies

Fetched automatically via CMake/vcpkg:
- CommonLibSSE-NG
- spdlog
- MinHook
- rapidcsv

## Version Bumping

When releasing, update version in all 6 locations:

1. `include/Version.h` - MAJOR, MINOR, PATCH
2. `CMakeLists.txt` - VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH
3. `vcpkg.json` - version-string
4. `version.rc` - FILEVERSION, PRODUCTVERSION, FileVersion, ProductVersion
5. `scripts/papyrus/source/SCIE_MCM.psc` - CurrentVersion integer
6. `scripts/papyrus/source/SCIE_MCM.psc` - About page version string

## Troubleshooting

### vcpkg not found
Ensure `VCPKG_ROOT` environment variable is set before running cmake.

### Build cache issues
Delete the `build` directory and reconfigure.

### Papyrus: SKI_ConfigBase not found
SkyUI SDK sources are missing. See "SkyUI SDK Setup" above.
