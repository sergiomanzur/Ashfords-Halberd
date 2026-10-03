# Native foot-lock and zombie motion-table implementation plan

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Execution:** Superpowers inline, tests first, one fresh source review.
**Goal:** Port Motion.c::bhCalcFixOffset and en01.c::CheckMtnTbl so the original
SetMtn can preserve planted feet and authored audiovisual timing.

## Boundaries

All verification stays headless; retail assets remain ignored/private.
SetMtn effects/root channel handling, action checks and combat remain required.
This increment does not register a replacement root AI or enable New Game.
Sound/vibration queues are source requests, not decoded audio or device output.

## Tasks

- [x] Add shared `src/ninja_matrix.hpp` float column translation/ZYX rotation/point helpers. Use them for native `calculate_actor_model_work` caches and existing enemy root-key translation. Write a cached-matrix literal and existing regression RED/GREEN: source quarter-table cosine and float rounding must agree with current-path foot lock. Keep renderer-only transforms separate.
- [x] Add generic `calculate_actor_fix_offset` in `src/actor.hpp` and new `src/actor_fix.cpp`: current complete pose + previous cached work + explicit placement + signed sentinel path + offset. Support linked parent matrix/local offsets, ignore primary1000 as source does, terminal cached joint subtraction, no mutation. Reject undefined empty/unterminated/out-of-range paths and invalid consumed transforms. Test RED/GREEN with literal positions/rotations, path padding, signed angles, linked and1000 cases, cached flags ignored and zero drift using actual model-work calculation.
- [x] Add `src/zombie_motion_table.cpp` and native constant tables from pinned MIT en01.c, public `tick_zombie_motion_table(state,work,frame,playing_codes)` and source vibration request queue/reset. Preserve room-bank pointer gate,40000 fix gate, motion2/80000 special path, all source fix types including mirror/125factor and leg/parent routing, inclusive windows and first valid match, all exact sound frame events, mode0<5 vibration gate/presets. Voice feedback represents current slot's active codes; any active sound suppresses positive queries before RNG, and nonpositive queries require an exact code. Stage actor/parent corrections, RNG and queues. Test RED/GREEN and source table integrity without retail fixtures.
- [x] Extend retail inspector with actual walking poses/cache foot-lock updates and native motion-table events; source review, full Windows build/CTest/package and fresh packaged dummy-SDL smoke, diff/format and commit. Keep full Step1 active.

## Files/interfaces

`src/ninja_matrix.hpp`, `src/actor_render.cpp`, `src/enemy_movement.cpp`,
`src/actor.hpp`, new `src/actor_fix.cpp`, new `src/zombie_motion_tables.hpp`,
new `src/zombie_motion_table.cpp`, `src/zombie.hpp`, `src/zombie_idle.cpp`,
`src/scenario.hpp`, `src/runtime.cpp`, tests and CMake. Small private scalar
correction results avoid scene/pose copies per callback. Source tables remain
typed immutable native constants with their original sentinels/order.

## Evidence anchors

Pinned MIT `Motion.c::bhCalcFixOffset`, `MdlPut.c::bhCalcTree/Model`,
`ps2_NaMatrix.c::njTranslate/RotateXYZ/CalcPoint`, `en01.c::CheckMtnTbl/SePlay`,
`zonzon.c::bhEne_SetVibration`. EXP0_I80 corresponds left leg15/16/17 and84
right leg12/13/14. Static table entries only use fix0/1/6/7/8, but source
switch also contains2/3/10/11/12/13. Source room-bank pointer identity gates all
fix/sound/vibration calls; source callback frame is captured before sampling.

## Verification

The fresh source review found one Important sound-occupancy mismatch; a
different same-slot active code reproduced it before the source rule was fixed.
The targeted regression and full39-test suite pass. All1,008 table integers
match pinned source. The retail scan passes205 rooms,480 source-float hierarchy
comparisons,1,610 isolated callbacks,4,800 zero-drift paths,28 sound requests
and10 vibration requests. The packaged dummy-SDL smoke passes241 ticks,
240 rendered frames,53 movement ticks,10 focus-blocked ticks and menu return.
The cached-matrix diagnostic now uses an independent child/sibling source-float
traversal, with exact equality rather than the separate double renderer math.
Full native SetMtn/effects, enemy combat, audio output and cross-room saves
remain required for Step1; New Game remains disabled.
