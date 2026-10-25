# Windows room viewer — 2026-10-02

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

Development build 0.2 adds Room Viewer to the main menu after ISO import.
It loads original room geometry/textures, displays a 640x480 native software
render inside a 4:3 viewport, and presents the result through SDL. Rendering
updates when the selected room or camera changes; this is not an animated
gameplay backend or an established frame-rate benchmark.

## Implemented

- Both authored camera definitions per cut, position/rotation/distance,
  perspective and MSB-first object-hide masks from the reference game.
- Perspective-correct texture interpolation, near/far and side-plane clipping.
- Camera mask selection can reveal model nodes originally flagged hidden.
  The diagnostic overview continues to exclude those initially hidden nodes.
- Active perspective degree bytes, height and roll are sampled at the cut
  midpoint. This is an explicit snapshot approximation. Full player tracking,
  angular limits, cut attributes, transitions and event cameras are pending.
- Rooms/cameras wrap through the archive; a second camera view is selectable.
  An unavailable camera falls back to an overview and is labeled accordingly.
- Viewer selections are saved as room/cut/variant under `[Viewer]` in the INI.
  Invalid values default safely; out-of-range archive selections normalize.
- Up/Down changes rooms, Left/Right changes cameras, Enter/A switches view,
  Escape/B returns to the menu. Controller D-pad and footer mouse buttons work.

## Verification

All eight CTest suites passed headlessly with SDL dummy video/software
rendering. The viewer fixture uses two synthetic rooms and two cuts, checks
actual colored pixels in the SDL framebuffer, exercises navigation/wrapping,
normalizes invalid saved selections and verifies persistence across relaunches
and return to the main menu. No original bytes are included in the fixture.

Camera tests cover mixed-axis transforms, active FOV, visibility masks,
near-plane crossing, geometry behind the camera, and a varying-depth texture
sample that distinguishes perspective-correct interpolation from affine UVs.

All 205 retail main-room scenes and their 1,072 cut records parsed successfully.
The installed Windows executable also displayed retail data under the dummy
video driver. Offline authored-camera images were inspected directly as files.
No visible application was launched for these checks.

This work corrected a pre-existing UV strip-scale reversal: Ninja type 65 uses
the reference Uvn scale (255), type 66 uses Uvh (1023). A regression covers both.
Code review identified the hidden-model overview regression; its test failed
before the fix and passed after it.

## Remaining

New Game stays marked in development. Actors, animation, collision, events,
movement, interactions and saves are not present. Lighting, transparency and
fog still approximate the original presentation; authored-camera snapshots
are not a reproduction of the complete dynamic camera system. The optional
RE2 Remake-style camera remains required in Step 2.

Next is player model/animation loading and adapting the original movement,
collision and camera-cut selection for the playable slice.
