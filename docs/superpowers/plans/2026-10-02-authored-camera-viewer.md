# Authored camera viewer

Continue approved Step 1 with a room viewer reachable from the native main menu.
All checks run headlessly using SDL dummy video/software rendering.

- Decode both camera variants per cut, position/angles/distance, perspective,
  and object-hide masks. Treat active perspective as four degree bytes rather
  than an ordinary integer. Resolve active snapshots at the cut midpoint;
  full player tracking and scripted transitions remain subsequent gameplay work.
- Add perspective-correct rasterization and bounded frustum clipping, preserving
  4:3 framing and the reference projection/aspect convention.
- Keep meshes which the authored visibility mask can reveal, even if their
  original model flags mark them hidden. Preserve the diagnostic overview.
- Add Room Viewer alongside the existing menu, with room/cut/variant selection
  persisted in the INI. Keep New Game marked in development.
- Verify binary boundaries, camera projection/clipping/masks, INI validation,
  synthetic SDL texture upload, real retail views and cross-room loading.

Approximate lighting, alpha threshold and missing actors are explicit viewer
limitations. This checkpoint does not claim a playable slice or modern camera.

Implemented camera definitions/snapshots, camera-space clipping/rasterization,
native menu integration and INI persistence. Tests first failed on missing
camera/settings APIs and the missing viewer smoke output. Retail camera
inspection exposed the earlier UV scale reversal; both strip types now have
a regression. Focused code review prompted the hidden-node overview fix and
expanded navigation/projection coverage. Eight suites passed headlessly.
