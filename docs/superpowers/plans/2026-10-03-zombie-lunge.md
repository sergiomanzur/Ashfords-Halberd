# Native zombie lunge and capture plan

> Execute inline with executing-plans/TDD; one final read-only source review.

**Goal:** Port complete en01.c::MV05 lunge and EatCheck dependencies for the
native graveyard combat controller.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Read original cached player/root joints; stage only work
motion/scalars, player grab flags/modes, linked body state and sound/RNG.
Keep sampling/cache updates separate. No asset or pose copies per action tick.
**Tech:** C++17/CMake/CTest, pinned en01.c/zonzon/njplus/NaColi source.

## Source decisions and constraints

- Headless only. Preserve default user profile and private retail files.
- EatCheck always computes cached joint3/joint10 planar distance after its
  initial flag gates, including mode1/2. Mode0 additionally uses actor-position
  search cone, strict distance, exact Y equality and health>=0; mode2 uses
  transformed joint10 sphere and the source sphere/capsule test.
- MV05 startup falls through to steering; frames8..12 may grab, frame12 miss
  enters recovery, and the final authored frame returns both works to walking.
- Player packed mode assignment means {4,0,0,4}; linked grab modes copy all four
  root mode bytes. Character1/3 uses121/321; other characters use85/285.
- Preserve pre-sampling action timing and existing sound suppression/RNG.
- New action-anchor vector represents EXP64/68/6c (shared source scratch).
- Grab damage/playback, root dispatcher, player bridge and complete combat
  remain required subsequent work; New Game stays disabled, Step1 active.

## Task1: native action and collision gates

Files: new src/zombie_attack.cpp, src/zombie.hpp, src/scenario.hpp,
new tests/zombie_attack_tests.cpp, CMakeLists.txt.

- [x] Add interfaces and named failing checks with pending implementations.
  Observe RED for capture gates and lunge startup.
- [x] Implement EatCheck all modes and literal npCollisionCheckSC arithmetic,
  then MV05 startup/steering/grab/recovery with transactional scalar staging.
- [x] Verify frame edges, character pairs, body mode copies, facing classification/Y/
  distance/contact boundaries, suppression/RNG and late-failure rollback.
  Exact two-facing Cdir angular boundary coverage is a deferred Minor; the
  current source arithmetic and tested facing classification are correct.

## Task2: original asset integration and verification

Files: tools/room_inspect.cpp, docs/scenario-status.md, this plan.

- [x] Run actual retail lunge motion→SetMtn→CalcEnemy for the required roots,
  verifying missed recovery and successful capture metadata on original banks.
  Preserve all existing205-room probes and no-op-free effect routing.
  Capture follows eight actual motion/cache frames, then places Claire's
  original bind hierarchy at the attack origin. It verifies this dependency
  with retail geometry, not interactive player animation or complete combat.
- [x] Final fresh read-only source review, material fixes RED-to-GREEN,
  full headless build/CTest, retail scan, fresh packaged dummy-SDL smoke and
  format/diff checks. Record evidence and commit.

Verified:51/51headless checks;205retail rooms;490lunge state updates,10capture
transitions after eight real animation/cache frames; existing probes retained.
Fresh packaged dummy-SDL:241ticks/240renders/53moves/10focus-blocked/menu/INI.
Important skipped-steering source finding fixed RED-to-GREEN; no remaining
material production findings. Deferred Minors: exact two-facing angular tests
and tightening the retail assertion to require both grab-flag bits.
