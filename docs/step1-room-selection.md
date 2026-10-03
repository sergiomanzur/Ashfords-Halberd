# Step 1 original room pair and integration priorities

Status: original room pair selected from the user's imported USA assets.
The native playable slice is still incomplete. New Game remains disabled.

## Selected rooms

Use Claire in stage0, rooms1 and2: `RM_0010.RDX` (corridor, RDX archive
entry1) and `RM_0020.RDX` (graveyard, entry2). Preserve the original graveyard
case change to `RM_0021.RDX` (entry3) when authored progression requests it.
No other campaign rooms are required to establish this two-room milestone.

Native archive expansion, record/collision decoding and a diagnostic runtime
entry were executed headlessly against the imported original files. The room
archive ordering is pinned in `ps2_dvd_image.c` and `room_archive_index`.

| Original room | Evidence from decoded data | Integration consequence |
| --- | --- | --- |
| Corridor | Two entry positions; one ID67 event actor; items31 and12; five interaction records | Retain existing actor/camera/event/movement behavior; complete standing and kneeling pickups and examination messages. |
| Corridor door0 | Type0, attribute128, parameters0,2,0,1 | Authored destination: stage0,room2,entry0. Existing examination/event probe already verifies this request. |
| Graveyard door1, both cases | Type0, attribute1, parameters0,1,1,1 | Reciprocal authored destination: stage0,room1,entry1. Preserve its locks and scripts; prove actual traversal rather than bypassing the door gate. |
| Graveyard case0 | Five ID1,type0 roots; model versions9 and32; one ID91 actor | Main root controller and story actor/event initialization must work together. |
| Graveyard case1 | Five ID1 roots: four type0 and one type1; model versions9,10,32 | Revisit/save restoration cannot substitute case0; type1 requires original linked fire behavior. |
| Graveyard, both cases | Five interactions: two doors, two pickups and one examination;14 floor records | Full original collision/trigger queries and both pickup modes remain necessary. |

The two selected rooms contain no miscellaneous stair triggers. MV08 is now
verified and reusable, but further unrelated campaign callbacks must not take
priority over completing this actual slice. Whether a floor/action path is
required is determined from its source conditions and original room data,
not merely from its presence in the full-campaign callback table.

## Current runtime blocker

Native ID1 startup is now integrated into the normal scenario scheduler with
retained player work. Both authored graveyard variants execute five roots,
five linked bodies and26auxiliaries in the same pool pass, including Init and
MainLoop's two motion samples and the frame collision/cache sequence.

Fresh native `load_room_runtime` entry for either graveyard variant now fails
at its original initialization script, after that enemy startup:

```
Unsupported scenario opcode 148 at 10
```

Opcode148 is original event.c::bhBGSeOn2, which calls CallBackGroundSe2 with
an unsigned byte and an unsigned short. Its audio dependency must be completed
without ignoring the command. Opcode106 EventSkipSet now preserves the original
gm_flg toggle. The corridor still enters normally;120original corridor ticks
verify retained player work, cache/capsule updates and source normal-anchor
timing separately from the front/movement anchors.

The native sound asset prerequisite now decodes the selected sound keys10/20/21,
including both graveyard ambient SQ sequences and their original instrument
samples. See docs/native-audio-status.md. This does not remove the148 boundary:
the original sequence synthesis, native mixer/fades and script playback bridge
still need to be implemented and verified through actual room entry.

The ordinary ID1 modes1..5 remain explicit unsupported states in the joined
controller; hit damage checking and first linked-fire birth also remain
unfinished. Existing movement/grab services must next be connected through
their actual action/event/damage dispatch. Source mode6 keeps its frame services
while bypassing Mode0 dispatch. No authored enemies or script commands are
skipped, and the actual corridor-to-graveyard transition still fails at148.

## Remaining acceptance sequence

1. Native original root/player scheduling in the selected rooms. Trace the
   original initialization and event paths through their actual scripts;
   retain fixed tick/RNG/motion/collision order and required effects.
2. Original armed controls, aiming/hit tests, damage, enemy death and player
   damage/death. Complete an actual encounter through the normal runtime,
   rather than isolated callback calls or relocated actors.
3. Pickup/message/inventory UI and equip/use behavior. Retain collection flags
   and authored puzzle/door conditions across the reciprocal connection.
4. Decode and mix required original music, voices and effects, retaining the
   separate volume settings. Prove decoded PCM and routing headlessly.
5. Versioned saves with player/inventory/event flags/room case and cross-room
   restoration. Test save in each room, restart, reload and revisit without
   duplicating pickups or losing progression.
6. Headless scripted traversal of both original rooms using production input,
   controller, render and audio paths. Capture rendered frames and traces;
   compare against original-game reference evidence. Preserve all milestone
   acceptance requirements before enabling New Game.

The optional modern over-the-shoulder camera remains required in Step2 and
does not replace original-camera fidelity or add work to Step1 acceptance.

## Audit provenance and limits

Local ignored diagnostic `local/slice_room_audit.cpp` uses production native
`afs_index`, `expand`, `decode_room_records`, `decode_room_collision` and
`load_room_runtime`, with original `SYSTEM.AFS` Claire package entry10.
It exited0 with `local/slice-room-audit.log`; each graveyard entry exception
is reported as an explicit block. No user desktop/profile was touched.

The subsequent ignored `local/zombie_runtime_entry_audit.cpp` exited0 with
`local/zombie-runtime-entry-final.log`:10roots/10bodies/52auxiliaries across
both original variants,120corridor work/cache ticks and60normal-anchor updates.
It explicitly reports both actual entry failures at opcode148; its successful
dependency probes do not establish a successful room transition.

This proves authored connectivity and asset/controller requirements. It does
not yet prove unlocked reciprocal travel, active combat, inventory, sound,
save/load or complete visual/reference fidelity. No proprietary bytes are
embedded in this document or committed diagnostic tools.
