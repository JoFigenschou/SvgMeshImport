# SvgMeshImporter (Unreal Engine 5.7)

Runtime and editor plugin that parses SVG paths, cleans geometry with Clipper2, triangulates caps with earcut, extrudes to procedural mesh geometry, optionally chamfers the top cap, and generates UVs.

## Installation

1. Copy or clone this repository into your Unreal project `Plugins/` folder, e.g. `YourProject/Plugins/SvgMeshImporter`.
2. Enable **Procedural Mesh Component** in your project (Edit → Plugins) if it is not already enabled.
3. Enable **SVG Mesh Importer** in Edit → Plugins.
4. Regenerate Visual Studio project files (right-click the `.uproject` → Generate Visual Studio project files).
5. Build the editor target.

Third-party sources live under `ThirdParty/` (nanosvg, earcut, Clipper2). Clipper2 is built from:

- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp`
- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp`
- `ThirdParty/Clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp`

Those files are included from `Source/SvgMeshImporterRuntime/Private/ThirdParty/Clipper2Impl.cpp` and validated in `SvgMeshImporterRuntime.Build.cs`.

## Supported SVG subset

- Filled paths from NanoSVG (`path`, `rect` converted to paths, basic shapes as paths)
- Cubic Bezier curves (flattened using **Curve Tolerance**)
- `fill-rule`: non-zero and even-odd (maps to **Winding Rule** in settings)
- Multiple shapes per file; union and hole detection via Clipper2
- No text, images, filters, gradients as mesh sources (ignored for geometry)
- Strokes are not extruded unless represented as filled paths

Sample files: `Content/Tests/SvgSamples/rect.svg`, `l_shape.svg`, `donut.svg`.

## Editor

**Tools → SVG Mesh Preview** opens a panel to pick an SVG file, run **Build Preview**, and read diagnostics (triangle counts, shape stats).

## Blueprint usage

### One-shot build

**Build From Svg File To Mesh** / **Build From Svg String To Mesh** apply geometry directly to a `UProceduralMeshComponent` and return success/failure.

1. Call **Build From Svg File** or **Build From Svg String** on `USvgMeshGenerator` (class **Svg Mesh Generator**).
2. Pass `FSvgMeshSettings` (extrude depth, chamfer, scale, flip Y, UVs, winding rule).
3. Use `FSvgMeshBuildResult.MeshData` vertices/triangles/normals/UVs, or spawn **Svg Mesh Actor**.

Example (Blueprint):

- Create object: `Svg Mesh Generator`
- **Build From Svg File** with path `Plugins/SvgMeshImporter/Content/Tests/SvgSamples/rect.svg`
- Feed result mesh data into your own procedural mesh setup

### Actor workflow

1. Place **Svg Mesh Actor** in the level.
2. Set **Svg File Path** (absolute or project-relative path readable at runtime).
3. Adjust **Mesh Settings**; enable **Build On Begin Play** for automatic rebuild.
4. Call **Rebuild Mesh** from Blueprint or enable begin-play build.

## C++ notes

- Module: `SvgMeshImporterRuntime` (game) and `SvgMeshImporterEditor` (preview UI).
- Exceptions are enabled for third-party headers (earcut, Clipper2).
- **Extrude Depth** keeps the top cap on the SVG plane (Z = 0) and extends side walls downward (default) or upward via **b Extrude Along Positive Z**.

## License

Plugin code: project license. Third-party libraries retain their respective licenses in `ThirdParty/`.

