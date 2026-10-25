# Alfred stairway investigation — 2026-10-24

## Conclusion and limits

The retail room script deliberately hides both Alfred records before playing the retreat sounds. This supports an original staging limitation exposed by a freer camera, rather than establishing a missing actor or a rendering regression. No actor was added, and no gameplay, renderer, configuration, save, or build files were changed for this investigation.

This is static evidence from the user's local USA PS2 disc and the pinned decomp. The scene has **not** been reproduced at runtime here. A separate rendering defect before the authored hide remains possible; that would need a scene save and visibility/flag observations. First-person issue #7 remains deferred.

## Issue evidence

[Issue #48](https://github.com/aikatore/recvx-enhanced-port/issues/48), opened 2026-10-21, is titled “enhancement: alfred is invisible” and labeled `enhancement`. Its report describes Alfred aiming a sniper rifle in a stairway encounter, followed by a door sound without a visible retreat. The [single maintainer comment](https://github.com/aikatore/recvx-enhanced-port/issues/48#issuecomment-6336938482) expresses uncertainty about feasibility; it does not identify a renderer bug, proposed implementation, room, or reproduction save.

## Scene identification

The matching retail script is `RM_3010.RDX`: stage 3, room 1, case 0, labeled “Military Training Facility: Relief stairway” in `campaign/pc/src/host/teleport.c:51`. Matching this to the issue is an inference from the stairway, Alfred records, and hide-then-sound sequence; the issue itself does not name the room.

The room contains these enemy/NPC records (zero-based):

| Record | ID | Work slot | Meaning |
|---|---:|---:|---|
| 0 | 91 | 0 | Claire cutscene model |
| 1 | 16 | 1 | Alfred gameplay actor |
| 2 | 94 | 2 | Alfred cutscene model |

IDs are supported by `campaign/pc/src/host/actors.c:16`, `campaign/pc/src/host/actors.c:24`, and `upstream/recvx-campaign-decomp/src/ps2/veronica/prog/en16.c:17`.

`RM_3011.RDX`, the later case of the same room, instead contains two Cerberus records and no Alfred. It is unsuitable for reproducing this encounter.

## Retail script evidence

Offsets below are byte offsets in the **expanded retail room**, as reported by the existing `campaign/pc/tools/rdx.py` disassembler. Its sweep completed cleanly for both 3010 and 3011.

| Offset | Script | Instruction / effect |
|---|---|---|
| `0x0bd2b2` | per-frame | Requires event flag 197 clear |
| `0x0bd2b8` | per-frame | Requires player floor trigger 5; starts events 2, 0, 1, 10, 3, 4, 14, 15, 16; then sets event flag 197 |
| `0x0bd540` | event 1 | `24 01 01 01`: shows Alfred record 1 |
| `0x0bd598` | event 1 | `24 01 01 00`: temporarily hides record 1 |
| `0x0bd5a6` | event 1 | `24 01 01 01`: shows record 1 again |
| `0x0bd8ae` | event 10 | Shows Alfred cutscene record 2 |
| `0x0bd91c` | event 10 | Hides record 2 |
| `0x0bd322` | per-frame | Requires event flag 198 clear |
| `0x0bd328` | per-frame | Requires player at `etc` trigger 0; starts event 11, then sets event flag 198 |
| `0x0bd92c` | event 11 | `24 01 01 00`: hides Alfred record 1 |
| `0x0bd930` | event 11 | `24 01 02 00`: hides Alfred record 2 |
| `0x0bd934` | event 11 | Loads work for record 0 (Claire), then `Sleep 50` |
| `0x0bd93c` | event 11 | Loop count 5, sound `0x0208` and `Sleep 7` |
| `0x0bd956` | event 11 | Sound `0x0208`, then music fade and `Sleep 20` |
| `0x0bd96c` | event 11 | Sound `0x0209`, then `SubCtr.128` and event end |

The complete event 11 contains no instruction to show Alfred again or animate a visible exit. Sound IDs are decoded operands; identifying their audible content as footsteps/door is not independently verified here. The issue supplies the door-sound description.

For locating the triggers, floor record 5 is at `(40, 0, 70)`, size approximately `(19.99, 0, 1.99)`. `etc` record 0 is at `(105, 0, 27)`, size approximately `(11.99, 0, 12.99)`, floor 0. These are retail script trigger records, not tested navigation directions.

## Why this is a real hide operation

`upstream/recvx-campaign-decomp/src/ps2/veronica/prog/event.c:2881` (`bhInitModelSet`) decodes `24 01 <record> 00` by resolving `rom->enep[record].wrk_no`, setting that enemy's `stflg |= 0x1000000` and `flg |= 0x8000`. A final operand of 1 clears the hide state.

`upstream/recvx-campaign-decomp/src/ps2/veronica/prog/eneset.c:953` (`bhDrawEnemy`) rejects actors whose `stflg & 0x41000000` is nonzero. This mask includes the script's `0x01000000` hide bit, before the PC variant/appearance draw hooks. Thus a renderer honoring this event is expected to omit Alfred.

The appearance catalogue supplies Alfred models; their existence is not evidence that the scene lacks an actor. `campaign/pc/glue/actor_glue.c:107` limits the player cutscene-double IDs to Claire/Chris/Steve/Wesker. `campaign/pc/glue/appearance_glue.c` manages local player appearance. These facts do not demonstrate a separate Alfred visibility bug. `campaign/pc/glue/tps_glue.c:803` also excludes script-hidden enemies from aiming; removing the hide flag could affect more than appearance.

## Reproduction requirement and next evidence

A normal playthrough must reach the training-facility relief stairway encounter; the opening cell-block save alone does not exercise it. A later save **before** this encounter is the practical runtime fixture. It must enter case 0 with event flags 197 and 198 clear to capture both the initial scene and retreat. A save after flag 198 is set, or case 1, will miss the transition. Teleporting alone is not an equivalent fixture because room/campaign state controls these events.

For a later authorized runtime check, compare the same pre-encounter save in fixed and third-person cameras. Observe records 1 and 2 before and after `etc` trigger 0, especially `stflg & 0x01000000`, active/model flags, position, and draw submission. An actor missing while the retail script says shown would support a rendering/activation investigation. An actor disappearing exactly at event 11 supports the staging explanation. The current evidence does not justify adding an actor or removing the original hide instructions.

## Reproducibility

Source provenance is recorded in `campaign/PROVENANCE.md`: campaign reference `38d457e3ec11a3120447e3544031af9a40eccbdb`, decomp pin `4aac5133901da3821fe443b79d6cdd1e3b29e6da` (also read from the local decomp checkout).

The local ISO was opened read-only through `rdx.Iso` / `rdx.Afs`. `RM_3010.RDX` is entry 49 in `RDX_LNK.AFS`; packed length 3,377,246 bytes; expanded length 7,568,200 bytes; expanded SHA-256 `b986687815f62df787d8dbf26f2f70a4a60933e68c91001cc14782459faf7579`. No proprietary room bytes were written into the repository.

The existing inspector's `room_names()` expects an older `campaign/decomp` layout. The read-only Python invocation imported `rdx`/`enemies`, replaced `rdx.room_names` in memory with names parsed from `upstream/recvx-campaign-decomp/src/ps2/veronica/prog/ps2_dvd_image.c`, and used `load_room`, `disasm`, `records`, and `enemies.walk`. No inspector source edits or new builds were needed.
