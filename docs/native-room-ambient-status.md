# Native original room ambient component

The native decoder reads ROM_WORK's four draw selectors and four planar RGB
float slots. Original command68 consumes six bytes and updates the selected
slot using source0.1f byte scaling, including overbright values. Malformed
extents, indices, selectors and nonfinite disk colors reject before publication.
Room entry binds destination colors before initialization; failed transitions
retain the current room and its colors. Script updates reach composed scenes
through current scenario state rather than a stale decoded snapshot.

Room, character, normal-object, half-light/special-object and item geometry now
carry the original draw-category selection. Composition follows normal object,
half-light object, special object and item passes before enemy/player drawing.
Source Ninja A/Da/As/Das material ambient BGR bytes survive decoding, including
commands after the last triangle. The rasterizer computes the actual ambient
component and retains material state across models and RuntimeView frames.
The development overview remains separate. Direct/half lights, normals,
specular, fog, full blend/effect rendering and PS2 pixel equivalence remain
required renderer work; ambient-only rendering is not complete presentation.

Source model bounds now gate ambient commands before polygon processing.
Decoded disk/native matrices transform the center and preserve the original
unscaled radius. Room/item and object drawing retain the source clipping policy;
main character drawing disables that sphere gate. Culled geometry cannot tint
later visible geometry or the next frame. Transparency, occlusion and lack of
pixels alone do not discard material commands. The current native view bounds
remain1..20000; original GameNear/ViewClipStage/script selection and dynamic
camera behavior still require their own source controllers and verification.

Eleven focused headless checks pass: source layout and command scaling, live
pixel updates, source material chunks, final channel saturation, category and
draw order, material persistence, failed transition recovery and destination
color binding, plus existing scene/camera/entry checks. Evidence:
local/room-ambient-focused-green.log. The prior production rasterizer produced
RGB70/140/175 instead of authored20/80/150 and discarded ambient material;
both behavioral failures were observed before implementation. Evidence:
local/room-ambient-rendering-red.log and local/room-ambient-materials-red.log.

Unchanged retail inspection finds corridor selectors0/1/0/0 and room/character
ambient0.2/0.3. Cemetery variants use selectors0/1/0/2 with ambient0.6/0.2/1.
Actual RDX2 command at404 is68,6,6,6,0,0. The unchanged native entry probe passes
both ambient commands and next requires command76 at416. RDX3 still requires
computer139 at276. RDX1 enters and advances120 ticks. There are no opcode skips
or forced branches. Evidence: local/room-ambient-source-inspect.log and
local/room-ambient-entry-audit.log. Cemetery entry and gameplay remain incomplete.

The corrected full Windows build, installation and package passed108/108
headless tests after one fresh read-only review found an Important sphere-clipping
issue. Both triangle/material-only failures were observed before the fix, then
five ambient checks and the full suite passed. Coverage includes all six bounds,
nonpositive radii, strict near/far touch, disk/native transforms, draw policy,
transparency/occlusion and invalid bound floats. No second reviewer was used.
Evidence: local/room-ambient-clipping-red.log,
local/room-ambient-clipping-final-red.log,
local/room-ambient-clipping-corrected-green.log and
local/room-ambient-review-fix-full-build.log.
The initial viewer fixture was updated to author white room
ambient for its existing red texture assertion, then that check and the whole
suite passed. A fresh packaged dummy runtime completed241ticks/240renders and
53movement ticks, generated192800 audio frames, submitted184000 and returned
with queue0. The ZIP has exactly six runtime/readme/license files and its EXE
matches dist. Evidence: local/room-ambient-review-fix-packaged-smoke.log and
local/room-ambient-review-fix-package-integrity.log. The unchanged post-fix
retail entry probe still enters RDX1 for120ticks and correctly rejects missing
76/416 and139/276 services: local/room-ambient-review-fix-entry-audit.log.

Complete Step1 still requires connected original gameplay, enemy encounter, combat,
inventory, full decoded sound/effects, versioned cross-room save/load and an
independent comparison with the original game. New Game remains disabled.
