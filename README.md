# SC4 Plop and Paint

A SimCity 4 plugin that adds an advanced lot and prop placement panel to the game.

## What it does

**In-game (`SC4PlopAndPaint.dll`):** Press **O** to open an ImGui panel with three tabs:
- **Buildings** — browse, filter, and plop lots organized by building
- **Props** — browse and paint individual props with line/polygon placement modes and palette management
- **Palettes** — create and manage weighted prop palettes for randomized placement

**Cache builder (`_SC4PlopAndPaintCacheBuilder.exe`):** Scans your SimCity 4 plugin directories, parses all exemplar/cohort records, and writes `lot_configs.cbor` and `props.cbor` to your Plugins folder. The in-game plugin reads these on city load.

## Installation

Download `SC4PlopAndPaint-{version}-Setup.exe` from the releases page and run it. The installer will:

1. Ask for your game root and Plugins directory
2. Place `SC4PlopAndPaint.dll`, `SC4CustomServices.dll`, and `SC4PlopAndPaint.dat` in your Plugins folder
3. Place `imgui.dll` in your SC4 `Apps` folder
4. Place `_SC4PlopAndPaintCacheBuilder.exe` and a generated `Rebuild-Cache.ps1` in `Documents\SimCity 4\SC4PlopAndPaint\`
5. Optionally run the cache builder immediately

To rebuild the cache later (e.g. after adding new plugins), run `Rebuild-Cache.ps1`.

If something looks wrong in-game, check `Documents\SimCity 4\SC4CustomServices.txt` for log output.

## Building from source

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/caspervg/sc4-advanced-plop
cd sc4-advanced-plop
```

**DLL (32-bit, Windows only — required for SC4):**

```bash
cmake --preset vs2022-win32-release
cmake --build --preset vs2022-win32-release-build --target SC4AdvancedLotPlop
```

**Cache builder CLI (64-bit, Windows):**

```bash
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release-build --target SC4AdvancedLotPlopCli
```

**macOS / Linux (CLI only):**

```bash
cmake --preset ninja-release
cmake --build --preset ninja-release-build
```

Use `ninja-debug` / `vs2022-win32-debug` / `vs2022-x64-debug` for debug builds.

Dependencies are managed via vcpkg (bundled in `vendor/vcpkg`). The CI workflow on Windows also builds `sc4-imgui-service` separately — see `.github/workflows/build.yml` for the full packaging steps.

**Running tests:**

```bash
ctest --preset ninja-debug
```

## Code overview

```
src/
├── shared/          # Header-only core library (SC4AdvancedLotPlopCore)
│   ├── entities.hpp     # Data structs: Building, Lot, Prop, PropPalette, etc.
│   └── tests/           # Catch2 unit tests
├── app/             # Cache builder CLI (SC4AdvancedLotPlopCli)
│   └── main.cpp         # Arg parsing, plugin scanning, CBOR export
└── dll/             # In-game plugin — Windows 32-bit only
    ├── SC4AdvancedLotPlopDirector.*  # GZCOM director: lifecycle, data loading, favorites
    ├── LotPlopPanel.*       # Top-level ImGui panel and tab management
    ├── BuildingsPanelTab.*  # Lot browser tab
    ├── PropPanelTab.*       # Prop browser tab
    ├── PalettesPanelTab.*   # Palette editor tab
    ├── PropPainterInputControl.*  # Mouse input for prop painting
    ├── PropLinePlacer.*     # Line-mode prop placement
    └── PropPolygonPlacer.*  # Polygon-fill prop placement
vendor/
├── DBPFKit/            # SC4 DBPF file format parser
├── gzcom-dll/          # GZCOM interface headers
├── reflect-cpp/        # Reflection/serialization (CBOR, JSON)
├── sc4-imgui-service/  # ImGui integration for SC4
└── vcpkg/              # Package manager
```

### Key design points

- **Shared entities** (`src/shared/entities.hpp`) are plain structs annotated with `rfl::Hex<T>` and `rfl::TaggedUnion` for automatic CBOR serialization via reflect-cpp.
- **Cache builder scanning** is two-pass: buildings are parsed first, then lot configs are resolved against the building map (including family-based growable lot references). Output is written as CBOR binary files.
- **DLL** loads the CBOR files on city init (`PostCityInit`), then renders the ImGui panel each frame. Prop painting uses a custom GZCOM input control and a draw service overlay for on-map feedback.
