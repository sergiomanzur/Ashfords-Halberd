# Native zombie walking and displacement implementation plan

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Execution:** Superpowers inline execution, tests first, one fresh source review.
**Goal:** Complete source wall probes, ikou/ikou3 turning, root-key displacement/speed and MV01 walking states as prerequisites for original MainLoop integration.

## Boundaries

All checks are headless. Retail assets stay private. These functions do not replace
SetMtn foot fix/effects, action checks, collision response, attacks or damage.
New Game remains disabled until the complete Step1 slice is ready.

## Tasks and interfaces

- [x] Add `src/enemy_wall.hpp/.cpp`, `ScenarioState::dynamic_walls`, room reset and tests. Query static walls before dynamic; retain exact types0–7, odd-type400 gate, raw type7 height, source strict horizontal/inclusive vertical boundaries. Forward/side probes use Ninja trig and supplied origins (caller can supply collision-offset origin). Return a pointer owned by the caller's immutable vectors.
- [x] Add `src/enemy_movement.hpp/.cpp`, `ScenarioEntity::speed`, tests. `turn_enemy_yaw` and `enemy_turn_step` retain signed32 word arithmetic, negative reverse target and linked80 no-op. `translate_enemy_motion`/`measure_enemy_motion_speed` consume current motion root position keys and pre-sampling frame, preserve inactive gate, mode0 Y removal, float ZYX transform and source truncating Y quantization. Reject consumed invalid data before mutation.
- [x] Add `src/zombie_walk.cpp`, short `turn_speed` (way), public `tick_zombie_walk`, tests and CMake entries. Stage only scalar motion/controller work and resolved sounds. Cover original root/body startup, RNG/fallthrough, strict attack and distance gates, walls/side choice, retained ayp, word yaw turn, stop windows/80000 leg flag, close-player turning and all sound cadence paths. Invalid consumed context must leave actor/body/RNG/queue unchanged. Share existing motion scalar helper with idle if useful.
- [x] Extend private retail inspector with isolated MV01 and actual root-key motion probes. Keep unsupported complete-root scheduler explicit. Update status and evidence, source review, full build/CTest/package, fresh packaged dummy-SDL smoke, diff/format and commit.

## Source anchors and verification

Pinned MIT `zonzon.c::ikou/ikou3/bhEne_GetTranslateMtn[2]/bhEne_CheckDirWall[2/3]/bhEne_CheckSideWall[2/3]`,
`hitchk.c::bhCheckWallType`, `en01.c::MV01`, `ps2_NaMatrix.c::njRotateXYZ/CalcPoint`.
Ninja uses Z,Y,X calls, float columns and source quarter-table trig. The scalar
speed helper does not translate an actor. Full SetMtn still owns zeroing root
joint channels and motion advancement. Synthetic tests assert authored literals
and branch boundaries; retail tests verify actual decoded channels and isolated
controller state without claiming full encounters.

## Verification result

Observed RED for pending wall/turn helpers and room dynamic-wall reset, then
GREEN3/3. MV01 stub failed before implementation, then walking and existing idle
regression passed2/2. Fresh source review found no Critical/Important issues and
two Minor unused-field validations. Re-graded against consumed-data requirements,
added failing regression assertions, moved radius validation out of type6 and
room-height validation out of nonzero-height triangles, and observed GREEN.

Final `tools/build.ps1` passed36/36CTest, installed the executable and rebuilt
the ZIP. Retail inspector passed205rooms/4363scenario slots,1,200 isolated MV01
ticks,1,272 real walking root-key translations and1,272 speed samples, retaining
previous corridor/route/awareness/idle/neck coverage. Fresh packaged dummy-SDL
smoke passed241ticks/240rendered frames/53movement ticks/10focus-blocked ticks,
menu return and INI persistence. Diff and clang-format checks passed. All checks
were headless. Full Step1 and original root MainLoop remain incomplete.
