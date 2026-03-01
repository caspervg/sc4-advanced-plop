# SC4 Plop and Paint

SC4 Plop and Paint is a SimCity 4 plugin that adds an in-game panel for browsing lots, painting props, and building reusable weighted prop families.

## What it does

`SC4PlopAndPaint.dll` adds an in-game window called `Advanced Plopping & Painting`. Press `O` in a loaded city to open it. The window is split into three tabs: `Buildings & Lots` for browsing and plopping lots, `Props` for painting individual props, and `Families` for managing and painting weighted random prop sets.

`_SC4PlopAndPaintCacheBuilder.exe` scans your SimCity 4 plugin directories, parses exemplar and cohort data, and writes `lot_configs.cbor` and `props.cbor` into your Plugins folder. The DLL reads those cache files when the city loads, which is why rebuilding the cache matters whenever your plugin collection changes.

## Installation

Install SC4 Render Services first, then download `SC4PlopAndPaint-{version}-Setup.exe` from the releases page and run it. The installer will:

1. Ask for your game root and Plugins directory.
2. Verify that `SC4RenderServices.dll` is already present in your Plugins folder.
3. Place `SC4PlopAndPaint.dll` and `SC4PlopAndPaint.dat` in your Plugins folder.
4. Place `_SC4PlopAndPaintCacheBuilder.exe` and a generated `Rebuild-Cache.ps1` in `Documents\SimCity 4\SC4PlopAndPaint\`.
5. Optionally run the cache builder immediately.

Required runtimes:

- Visual C++ 2015-2022 Redistributable (x86, required for SimCity 4 / 32-bit): `https://aka.ms/vs/17/release/vc_redist.x86.exe`
- Visual C++ 2015-2022 Redistributable (x64): `https://aka.ms/vs/17/release/vc_redist.x64.exe`

To rebuild the cache later, for example after adding or removing plugins, run `Rebuild-Cache.ps1`.

If something looks wrong in game, check the separate services plugin's log output in `Documents\SimCity 4\`.

## Using it in-game

This section is written for players, not developers. It covers the full in-game flow after installation and after the cache has been built.

### Before you start

1. Run the cache builder if you have added, removed, or changed plugins.
2. Start SimCity 4 and load a city.
3. Press `O` to open the `Advanced Plopping & Painting` window.

If the panel opens but the lists are empty, `Buildings & Lots` is missing `lot_configs.cbor`, or `Props` / `Families` are missing `props.cbor`.

### The tabs

**Buildings & Lots**

This tab is for finding and plopping lots. The upper part of the window contains filters for search text, zone type, wealth, growth stage, lot width and depth, favorites, and occupant groups. The main table shows buildings, and selecting a building reveals its attached lots in the lower table. Double-click a lot row or press `Plop` to trigger that lot in game. `Star` and `Unstar` let you maintain lot favorites.

**Props**

This tab is for individual props. You can search by name, filter by prop dimensions, and optionally show favorites only. Press `Paint` to start painting that prop. `Star` and `Unstar` manage prop favorites, and `+` adds a prop to one of your custom families.

**Families**

This tab combines built-in prop families loaded from the cache with your own custom families. You can search by family name or by IID, create a new family, rename or delete your own families, adjust entry weights, remove entries, tweak `Density variation`, and start painting with a family as a weighted random palette.

### Favorites

Favorites are there to reduce repeated searching.

In `Buildings & Lots`, press `Star` on a lot to save it and `Unstar` to remove it. Turning on `Favorites only` limits the visible list to your saved lot favorites that still match the other filters.

In `Props`, `Star`, `Unstar`, and `Favorites only` work the same way for props.

Favorites are stored by the plugin and persist between sessions.

### Creating and managing your own families

Custom families are your own weighted random prop palettes.

To create one, open `Families`, press `New family`, enter a name, and press `Create`. The new family starts empty.

The fastest way to add props is from the `Props` tab: find a prop, press `+`, and choose the destination family from the popup. Once a family has members, return to `Families` to review and edit it.

When a custom family is selected, you can rename it, delete it, change each entry's weight, remove individual entries with the `x` button, and adjust `Density variation`. Higher weights make an entry more likely to be chosen during family painting.

Built-in families are read-only. Custom families are fully editable.

### Starting paint mode

Painting can be started from the `Props` tab for a single fixed prop or from the `Families` tab for a weighted random family. Before the tool activates, a popup lets you choose the paint settings.

Common options:

- `Mode`: `Direct paint`, `Paint along line`, or `Paint inside polygon`
- `Rotation`: fixed 0 / 90 / 180 / 270 degrees
- `Show grid overlay`: shows the preview grid on the terrain
- `Snap points to grid`: snaps the points you click to the grid
- `Also snap placements to grid`: snaps final placed props to the grid as well
- `Grid step (m)`: grid size in meters

Line mode adds `Spacing (m)`, `Align to path direction`, and `Lateral jitter (m)`. Polygon mode adds `Density (/100 m^2)` and `Random rotation`.

When painting with a family, each placed prop is chosen from that family using its saved weights.

### How the paint tool works

After you press `Start`, the paint tool takes over map input until you commit or cancel.

In `Direct paint`, left-click places one prop at a time.

In `Paint along line`, left-click adds line points and `Enter` places props along the path.

In `Paint inside polygon`, left-click adds polygon vertices and `Enter` fills the area.

### Paint tool controls

While paint mode is active:

- `R`: rotate the fixed rotation (cycles 0 / 90 / 180 / 270)
- `P`: toggle the preview overlay
- `Backspace`: remove the last unconfirmed line point or polygon vertex while drawing
- `Ctrl+Z`: undo the whole last placement group
- `Ctrl+Backspace`: undo only the last prop in the current top group
- `Enter`: place the current line or polygon batch, or commit pending placements
- `Esc`: cancel all pending placements and leave paint mode

### What "pending placements" means

Painted props are first placed in a temporary highlighted state. They are visible immediately, but they are still considered pending until you commit them. While they are pending, you can undo them. Press `Enter` to commit everything currently pending, or press `Esc` to remove all pending placements instead.

That means even in direct mode there are usually two stages: you click to place one or more props, then you press `Enter` when you are satisfied and want to make them final.

### Typical flows

**Plop a lot**

1. Press `O`.
2. Open `Buildings & Lots`.
3. Filter until you find the building or lot you want.
4. Double-click the lot row or press `Plop`.

**Paint one prop repeatedly**

1. Press `O`.
2. Open `Props`.
3. Find the prop and press `Paint`.
4. Choose `Direct paint` or another mode and set the options you want.
5. Press `Start`.
6. Click to place props.
7. Use `Ctrl+Z` or `Ctrl+Backspace` if needed.
8. Press `Enter` to commit, or `Esc` to cancel.

**Paint a row of props**

1. Start from `Props` or `Families`.
2. Choose `Paint along line`.
3. Press `Start`.
4. Click each control point along the line.
5. Press `Enter` to generate the placements.
6. Press `Enter` again to commit them, or undo or cancel instead.

**Paint a random family**

1. Open `Families`.
2. Select or create a family.
3. Adjust the family weights if needed.
4. Press `Paint family`.
5. Choose a mode and options.
6. Paint as normal; each placement will be chosen from the family.

### Stopping paint mode

Press `Esc` to cancel pending placements and leave paint mode immediately. If you reopen the panel while painting, the `Props` and `Families` tabs also show a `Stop painting` button.

## Building from source

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/caspervg/sc4-advanced-plop
cd sc4-advanced-plop
```

**DLL (32-bit, Windows only - required for SC4):**

```bash
cmake --preset vs2022-win32-release
cmake --build --preset vs2022-win32-release-build --target SC4PlopAndPaint
```

**Cache builder CLI (64-bit, Windows):**

```bash
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release-build --target SC4PlopAndPaintCli
```

**macOS / Linux (CLI only):**

```bash
cmake --preset ninja-release
cmake --build --preset ninja-release-build
```

Use `ninja-debug`, `vs2022-win32-debug`, or `vs2022-x64-debug` for debug builds.

Dependencies are managed via vcpkg (bundled in `vendor/vcpkg`). On Windows, the CI workflow also builds `sc4-imgui-service` separately before packaging the release artifact.

**Running tests:**

```bash
ctest -C Debug --test-dir cmake-build-debug-visual-studio --output-on-failure
```

## Code overview

```text
src/
|- shared/          # Header-only core library (SC4PlopAndPaintCore)
|  |- entities.hpp     # Data structs: Building, Lot, Prop, FamilyEntry, favorites, etc.
|  `- tests/           # Catch2 unit tests
|- app/             # Cache builder CLI (SC4PlopAndPaintCli)
|  `- main.cpp         # Arg parsing, plugin scanning, CBOR export
`- dll/             # In-game plugin - Windows 32-bit only
   |- SC4PlopAndPaintDirector.*   # GZCOM director: lifecycle, data loading, favorites
   |- LotPlopPanel.*              # Top-level ImGui panel and tab management
   |- BuildingsPanelTab.*         # Lot browser tab
   |- PropPanelTab.*              # Prop browser tab
   |- FamiliesPanelTab.*          # Family browser and editor tab
   |- PropPainterInputControl.*   # Mouse/keyboard input for prop painting
   |- PropLinePlacer.*            # Line-mode prop placement
   `- PropPolygonPlacer.*         # Polygon-fill prop placement
vendor/
|- DBPFKit/            # SC4 DBPF file format parser
|- gzcom-dll/          # GZCOM interface headers
|- reflect-cpp/        # Reflection/serialization (CBOR, JSON)
|- sc4-imgui-service/  # ImGui integration for SC4
`- vcpkg/              # Package manager
```

### Key design points

- **Shared entities** (`src/shared/entities.hpp`) are plain structs annotated with `rfl::Hex<T>` and `rfl::TaggedUnion` for automatic CBOR serialization via reflect-cpp.
- **Cache builder scanning** is two-pass: buildings are parsed first, then lot configs are resolved against the building map, including family-based growable lot references. Output is written as CBOR binary files.
- **DLL** loads the CBOR files on city init (`PostCityInit`), then renders the ImGui panel each frame. Prop painting uses a custom GZCOM input control and a draw service overlay for on-map feedback.

## Third-party code

| Library | Purpose | License |
|---|---|---|
| [sc4-render-services](https://github.com/caspervg/sc4-render-services) | ImGui backend and SC4 custom services integration | LGPL 2.1 |
| [gzcom-dll](https://github.com/nsgomez/gzcom-dll) | GZCOM interface headers for SC4 plugin development | LGPL 2.1 |
| [DBPFKit](https://github.com/caspervg/DBPFKit) | DBPF archive reader (exemplars, FSH, S3D, LText) | - |
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode UI framework | MIT |
| [reflect-cpp](https://github.com/getml/reflect-cpp) | Compile-time reflection and CBOR/JSON serialization | MIT |
| [spdlog](https://github.com/gabime/spdlog) | Structured logging | MIT |
| [args](https://github.com/Taywee/args) | CLI argument parsing | MIT |
| [pugixml](https://github.com/zeux/pugixml) | XML parsing (PropertyMapper) | MIT |
| [yyjson](https://github.com/ibireme/yyjson) | Fast JSON parsing (reflect-cpp backend) | MIT |
| [stb](https://github.com/nothings/stb) | Image decoding/encoding | MIT / Public Domain |
| [WIL](https://github.com/microsoft/wil) | Windows Implementation Library helpers | MIT |
| [libsquish](https://sourceforge.net/projects/libsquish/) | DXT texture decompression | MIT |
| [mio](https://github.com/mandreyel/mio) | Memory-mapped file I/O | MIT |
| [jsoncons](https://github.com/danielaparker/jsoncons) | JSON/CBOR processing | Boost 1.0 |
| [utfcpp](https://github.com/nemtrif/utfcpp) | UTF-8 string utilities | Boost 1.0 |
| [ctre](https://github.com/hanickadot/compile-time-regular-expressions) | Compile-time regular expressions | Apache 2.0 |
| [raylib](https://www.raylib.com) | 3D rendering for thumbnail generation | zlib |
| [GLFW](https://www.glfw.org) | OpenGL windowing (raylib dependency) | zlib |

Full license texts are in [dist/ThirdPartyNotices.txt](dist/ThirdPartyNotices.txt).
