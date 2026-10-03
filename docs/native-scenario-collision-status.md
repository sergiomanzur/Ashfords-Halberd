# Native original scenario collision activation

Original commands 10, 11 and 12 now enable or disable the selected static
wall, miscellaneous or floor record. Their four-byte cursor and zero/nonzero
enable/disable semantics follow event.c; only flag bit 1 changes. Invalid
instruction extents, indices and room bindings reject before publication.

Miscellaneous commands synchronize collision and interaction views, including
existing commands 14, 35 and 37. Bound commands read the live collision record,
preserving stair reservations changed directly by actor controllers instead
of publishing stale interaction fields. Player movement consumes the current
scenario collision data, so original script changes reach wall and floor
queries instead of leaving the decoded room snapshot active. Enemy queries
already use this current data.

The unchanged retail production probe passes RDX2's command 11 at byte 364 and
the following wall changes. It next requires command 68 at byte 404,
bhEffAmbSet. RDX3 still requires command 139 at byte 276, computer event 0.
RDX1 enters and advances 120 ticks. No opcode skips or forced script branches
were used. Cemetery entry and the connected gameplay slice remain incomplete.
Evidence: local/scenario-collision-final-entry-audit.log.

Seven focused checks passed after correcting the runtime fixture to initialize
visual_anchor as the original room entry does. The corrected fixture fails
against unchanged BASE runtime code with two stale wall hits, then passes
against current production with zero hits. The full Windows build, installation
and package passed 103/103 headless tests. A fresh packaged dummy runtime
completed 241 ticks, 240 renders and 53 movement ticks, generated 192800 audio
frames, submitted 184000 and returned with no queued audio. The ZIP contains
only the six executable/readme/license files and its executable matches dist.
Evidence: local/scenario-collision-focused-corrected-green.log,
local/scenario-collision-final-full-build.log,
local/scenario-collision-final-packaged-smoke.log and
local/scenario-collision-final-package-integrity.log.

One fresh read-only review found no Critical or Minor issues and one Important
integration defect: a stale interaction copy could overwrite a live stair
reservation. A regression using actual actor reservation/release services and
enemy stair queries failed before the correction and passed afterward. The
author's single fix pass passed seven focused checks and the full 103-check
suite, then repeated the unchanged retail probe and fresh packaged smoke.
No second review was dispatched. Evidence:
local/scenario-collision-reservation-red.log,
local/scenario-collision-reservation-focused-green.log and
local/native-scenario-collision-activation-progress.md.

Complete Step 1 still requires the two connected
rooms, original player/enemy gameplay, combat, inventory, complete sound and
effect presentation, versioned cross-room save/load and independent comparison
with the original game. New Game remains disabled. The modern over-the-shoulder
camera belongs to Step 2.
