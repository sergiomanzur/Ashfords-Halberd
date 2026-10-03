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

Fresh native `load_room_runtime` entry for either graveyard variant fails at
`initialize_scenario -> tick_scenario_actors`:

```
Unsupported scenario actor controller: room 2 actor 1 mode 0
```

The corridor entry succeeds and binds its original actor, items and triggers.
Separately tested zombie startup/movement/grab services do not satisfy this
ordinary scheduler call. The immediate integration priority is the source
ID1 frame sequence and player work/caches, including root and linked body
ordering, required action dispatch, player damage responses and event modes.
Do not suppress the exception with a no-op or skip the authored enemies.

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

This proves authored connectivity and asset/controller requirements. It does
not yet prove unlocked reciprocal travel, active combat, inventory, sound,
save/load or complete visual/reference fidelity. No proprietary bytes are
embedded in this document or committed diagnostic tools.
