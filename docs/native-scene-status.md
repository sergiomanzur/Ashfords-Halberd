# Native scene decoding — 2026-10-02

This increment adds a portable C++ scene decoder and a native, headless software
rasterizer. The Windows game executable still contains the foundation menu;
these development tools are not a playable game mode.

## Verified coverage

- All 205 main-room scenes in the user's USA RDX_LNK.AFS decode successfully.
- Their decoded meshes contain 1,008,630 triangles in total, with 9,425 texture
  instances across the room packs. These are not counts of unique assets.
- The first room produces 67 meshes, 5,502 triangles and 32 textures.
- PS2 Ninja type-51 vertices, indexed strips (64/65/66), UVs, diffuse material
  records, and object transforms/hierarchy are read through bounded offsets.
- TIM2 indexed 4-bit/8-bit and RGBA32 pixels become portable RGBA images, with
  palette order and PS2 alpha conversion. Prepared GPU transfer formats and
  palette-change records are rejected.
- Untextured geometry is supported explicitly, including room index 87.

## Headless tools

Build with `tools/build.ps1`. Both tools run without creating a window:

```powershell
./build/windows/AshfordRoomInspect.exe 'path/to/assets/RDX_LNK.AFS' 'local/scenes.tsv' --scene
./build/windows/AshfordScenePreview.exe 'path/to/assets/RDX_LNK.AFS' 0 'local/new-scene-output'
```

The preview requires a new output directory and saves `overview.ppm`, texture
PPMs and `summary.txt`. It never replaces existing files. The overview uses a
diagnostic orthographic camera, a depth buffer, UV sampling, an alpha threshold
and approximate lighting. It is useful for validating decoded geometry and
texture placement, not for comparing the original game's final presentation.
No original assets or generated previews enter Git or the Windows ZIP.

## Checks and limits

Six CTest suites cover asset, scene, raster, settings/disc, headless menu and
packaged-runtime behavior. Scene tests include strip padding, invalid indices,
invalid pointers, cyclic links, transforms, palette/alpha conversion and 1,500
deterministic model mutations. Raster tests check transparency, repeatable
output and depth occlusion independent of triangle submission order.

A focused code review caught material BGRA ordering and the 4-bit CSM1 palette
mapping. Both were corrected, with regression cases verified to fail against
the previous implementations and pass against the corrected decoder.

The scene suite also passed with GCC standard-library assertions enabled.
An UndefinedBehaviorSanitizer build was attempted but this installed MinGW
toolchain lacks `libubsan`; no sanitizer pass is claimed.

Only main-room models have been validated across the archive. Character models,
animation, additional chunk/texture encodings, authored camera behavior,
visibility masks, original lighting/fog and transparency blending still need
implementation and verification. Current strips retain source winding, but
the diagnostic render draws both sides and does not reproduce render states.

Next: integrate room loading with a real-time native backend, reproduce an
authored room camera and its visibility masks, then adapt player animation,
collision, events and room transitions for the playable slice.
