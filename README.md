# SvgMeshImporter (Unreal Engine 5.7)

Runtime and editor plugin that parses SVG paths, cleans geometry with Clipper2, triangulates caps with earcut, extrudes to procedural mesh geometry, optionally chamfers the top cap, and generates UVs.

The primary workflow is **Svg Mesh Actor**: import an SVG, build one mesh per shape, optionally bake static mesh assets in the editor, and reposition the whole map by moving the actor.

## Features

- SVG parsing via NanoSVG (filled paths, basic shapes, rects as paths)
- Path cleanup, union, and hole detection with Clipper2
- Cap triangulation with earcut (including holes when **Cut Holes** is enabled)
- Extrusion with configurable depth, direction, side-wall normals, and tangent-space modes
- Optional top-cap chamfer
- UV generation and mesh centering
- Per-shape procedural mesh components, or editor-time static mesh baking
- Simple axis-aligned box collision (not per-triangle complex collision)
- Engine default material fallback when **Mesh Material** is not set

## Installation

1. Copy or clone this repository into your Unreal project `Plugins/` folder, e.g. `YourProject/Plugins/SvgMeshImporter`.
2. Enable **Procedural Mesh Component** in your project (Edit → Plugins) if it is not already enabled.
3. Enable **SVG Mesh Importer** in Edit → Plugins.
4. Regenerate Visual Studio project files (right-click the `.uproject` → Generate Visual Studio project files).
5. Build the editor target.

The plugin also depends on **Editor Scripting Utilities** (enabled via `SvgMeshImporter.uplugin`).

Third-party sources live under `ThirdParty/` (nanosvg, earcut, Clipper2). Clipper2 is built from:

- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp`
- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp`
- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp`

Those files are included from `Source/SvgMeshImporterRuntime/Private/ThirdParty/Clipper2Impl.cpp` and validated in `SvgMeshImporterRuntime.Build.cs`.

## Supported SVG subset

- Filled paths from NanoSVG (`path`, `rect` converted to paths, basic shapes as paths)
- Cubic Bezier curves (flattened using **Curve Tolerance**)
- `fill-rule`: non-zero and even-odd (maps to **Winding Rule** in settings)
- Multiple shapes per file; each filled path can become its own mesh
- Inner contours detected as holes when **Cut Holes** is enabled (e.g. donut, letter O)
- Optional shape union via **Union Shapes** (auto-disabled for multi-path files)
- No text, images, filters, or gradients as mesh sources (ignored for geometry)
- Strokes are not extruded unless represented as filled paths

Sample files: `Content/Tests/SvgSamples/rect.svg`, `l_shape.svg`, `donut.svg`.

## Svg Mesh Actor workflow

1. Place **Svg Mesh Actor** in the level.
2. Set **Svg File Path** (absolute or project-relative path readable at runtime).
3. Adjust **Mesh Settings** as needed.
4. Optionally assign **Mesh Material**; if left empty, the engine default surface material is used.
5. Click **Generate Mesh** in the Details panel, call **Rebuild Mesh** from Blueprint, or enable **Build On Begin Play**.

### Actor properties (defaults)

| Property | Default | Notes |
|---|---|---|
| **Mesh Material** | none | Falls back to engine default surface material |
| **Build On Begin Play** | `false` | Auto-rebuild at runtime when enabled |
| **Create Collision** | `true` | Simple box collision from mesh bounds |
| **Bake To Static Meshes** | `true` | Editor-only; bakes one static mesh asset per shape |
| **Static Mesh Output Path** | `/Game/SvgMeshImporter/Generated` | Content folder for baked assets |

After rebuild:

- Multi-shape SVGs produce one component per shape under the actor root.
- With baking enabled, shapes become movable static mesh components backed by generated assets.
- With baking disabled, shapes use procedural mesh components instead.
- Move the actor (not individual shape children) to reposition the imported map.

## Mesh settings (defaults)

| Setting | Default | Notes |
|---|---|---|
| **Extrude Depth** | `10` | SVG units, multiplied by **Svg Scale**; top cap stays at Z = 0 |
| **Extrude Along Positive Z** | `false` | `false` = extrude along -Z; `true` = along +Z |
| **Flip Extrusion Sides** | `true` | Flip side-wall winding/normals if faces look inside-out |
| **Tangent Space Mode** | Negate Side Normals | Debug recipes for side-wall lighting |
| **Generate Smooth Normals** | `false` | Smooth side walls only; caps stay flat |
| **Chamfer Distance** | `0` | `0` = off |
| **Chamfer Segments** | `4` | |
| **Curve Tolerance** | `0.25` | Bezier flattening tolerance |
| **Simplify Tolerance** | `0` | `0` = preserve full detail |
| **Min Edge Length** | `0` | `0` = keep all segments |
| **Min Shape Area** | `0` | `0` = keep all shapes |
| **Svg Scale** | `1` | |
| **Generate UVs** | `true` | |
| **Flip Y** | `true` | SVG Y-down to Unreal coordinates |
| **Flip X** | `true` | |
| **Winding Rule** | Non-Zero | |
| **Union Shapes** | `false` | Auto-disabled when file has multiple filled paths |
| **Cut Holes** | `true` | Inner contours become holes |
| **Auto Separate Path Threshold** | `2` | Min filled paths before union is auto-disabled |
| **Center Mesh** | `true` | Center mesh on actor origin from SVG bounds |

## Editor

**Tools → SVG Mesh Preview** opens a panel to pick an SVG file, run **Build Preview**, and read diagnostics (triangle counts, shape stats).

## Blueprint usage

### One-shot build

**Build From Svg File To Mesh** / **Build From Svg String To Mesh** apply geometry directly to a `UProceduralMeshComponent` and return success/failure.

1. Call **Build From Svg File** or **Build From Svg String** on `USvgMeshGenerator` (class **Svg Mesh Generator**).
2. Pass `FSvgMeshSettings` (extrude depth, chamfer, scale, flip axes, UVs, winding rule, hole cutting, etc.).
3. Use `FSvgMeshBuildResult.MeshData` or `FSvgMeshBuildResult.ShapeMeshes`, or spawn **Svg Mesh Actor**.

Example (Blueprint):

- Create object: `Svg Mesh Generator`
- **Build From Svg File** with path `Plugins/SvgMeshImporter/Content/Tests/SvgSamples/rect.svg`
- Feed result mesh data into your own procedural mesh setup

`BuildFromSvgFileToMesh` / `BuildFromSvgStringToMesh` accept an optional `bCreateCollision` argument (default `true`). Collision uses a simple bounding box, not complex mesh collision.

## Development scripts

The `scripts/` folder contains PowerShell helpers for local development:

- `sync-to-unreal.ps1` — mirror source into an Unreal project's plugin folder
- `build-plugin.ps1` — build the plugin with UnrealBuildTool (`-Sync` runs sync first)
- `install-git-hooks.ps1` — install git hooks that sync after commit/push

Edit the default paths at the top of each script for your machine.

## C++ notes

- Modules: `SvgMeshImporterRuntime` (game/runtime) and `SvgMeshImporterEditor` (preview UI, static mesh baking).
- Exceptions are enabled for third-party headers (earcut, Clipper2).
- **Extrude Depth** keeps the top cap on the SVG plane (Z = 0) and extends side walls along -Z by default (or +Z when **Extrude Along Positive Z** is enabled).
- Collision is applied as a simple `FKBoxElem` from vertex bounds; `bUseComplexAsSimpleCollision` is not used.
- Key types: `ASvgMeshActor`, `USvgMeshGenerator`, `FSvgMeshSettings`, `FSvgMeshBuildResult`, `FSvgShapeMesh`.

## License

Plugin code: project license. Third-party libraries retain their respective licenses in `ThirdParty/`.
