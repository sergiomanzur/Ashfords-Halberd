# Native scene decoding checkpoint

Continue Step 1 asset access/rendering under the approved project scope.
Keep all checks headless and all original bytes under ignored local paths.

1. Decode main-room Ninja chunk model vertices, strip indices/UVs, materials,
   and object hierarchy through explicit file-relative offsets.
2. Decode the room texture pack's TIM2 records into portable RGBA images.
3. Reject unsupported encodings and invalid ranges rather than guessing.
4. Test synthetic strips, bad indices/pointers, hierarchy cycles, palette
   conversion and malformed textures before testing the user's real rooms.
5. Build a development-only native scene exporter/rasterizer. Save an offline
   overview, inspect its pixels, and validate room coverage headlessly.

This checkpoint is a rendering foundation, not player movement or gameplay.
An overview camera is a diagnostic; authored camera behavior remains part of
the playable slice. No additional runtime dependency is required.

Implemented scene and raster APIs, two boundary suites, a new native preview
tool, and the room inspector's --scene mode. Initial test compilation failed
on missing APIs. Regression tests reproduced strip-padding, untextured-geometry
and TIM2 format cases before their fixes. All 205 main-room scenes decoded;
the first room was rendered and its saved pixels inspected without a window.
Full headless verification and a focused code review precede the checkpoint.
