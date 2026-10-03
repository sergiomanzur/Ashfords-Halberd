# Native zombie frame collision implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development;
> one fresh source review at the final gate, no per-task implementer agents.

**Goal:** Complete original en01.c CollCheck/CollCheckPush/CollCheckWall so the
selected corridor/graveyard runtime can consume its required enemy-frame
collision sequence without omitting contact or posture-specific wall response.
**Architecture:** Stage compact actor/contact metadata, player scalars and room
wall/effect mutations. Read immutable original pose/cache owners by reference;
rebuild only the exact source parts path. Existing native contacts, line query,
step queries and stateful WallEx/Wall2 supply the required response services.
**Tech Stack:** C++17/CMake/CTest, pinned MIT source, Windows x64.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Selected slice:** docs/step1-room-selection.md.
**Base:** 81ab1f2 clean,67checks/current original scan/package/smoke passed.

## Global constraints and review focus

- Headless only; fresh diagnostic profiles and imported USA assets ignored.
- Preserve source fixed ticks, float/word arithmetic and static/dynamic order.
- No complete actor/model/pose copies in this per-frame callback.
- Linked body collision acts on its original parent; owner cache may be shared.
- Push clears bit2 on active unlinked work, preserves source pusher-body mode0
  write, hit-test OR predicate and whit-conditioned position restoration.
- Parent40000 forces secondary bit1 after CheckPlayer and suppresses pool query.
- Line query is always updated by CollCheckWall, even when shape0 has no response.
- Wall shapes1..6 consume their exact current-pose paths and prior caches;
  sequential probes retain source precomputed coordinates and updated work.
- Floor effect is first inclusive active/type3/current-floor XZ match; water
  uses existing original XYZ/static-dynamic predicate and guards.
- Late consumed failures commit no root/other/player/wall/effect changes;
  unused absent/nonfinite pose/cache/player data remain ignored.

## Task1: callback and literal source fixtures

Files: new src/zombie_collision.cpp,src/zombie.hpp,src/scenario.hpp,
new tests/zombie_collision_tests.cpp,CMakeLists.txt.

Interface:
```cpp
void tick_zombie_collision(ScenarioState&,size_t,ScenarioEntity& player);
```
Consumes collide_enemy_player/collide_actor_pool, query_collision_line,
query_water, check_step_state, collide_stateful_walls_ex/check_enemy_wall2,
enemy_direction and native float Ninja matrix operations. Reuses existing
collision offsets/damage_velocity/modes/zombie flags and path_blocked storage.

- [x] Add pending callback/new CTest zombie-frame-collision; observe named
  FAIL before implementation.
- [x] Literal fixtures: inactive flags, unlinked root vs linked-body parent,
  player split contact and forced secondary1, pool overlap flags, push hit
  front/back mode2 and original pusher-body mode0, whit restoration, path0
  line query, all seven wall shapes, branch-specific pose/cache requirements,
  source prior-Y overrides and sequential probe displacement, floor/water
  first-match/gates, aliases/bounds and late failure rollback.
- [x] Implement complete three source functions with compact staging; source
  part paths use en01_tree rows2/3/7 with exact explicit lengths.
- [x] Run focused new collision + existing contact/stateful/line checks.
  Expected: all pass, no compiler warnings, no asset/pose copies.

## Task2: original asset collision frame paths and final gates

Files: new tools/zombie_collision_probe.cpp/.hpp,tools/room_inspect.cpp,
CMakeLists.txt,docs/scenario-status.md,this plan.

- [x] Retain both initialized original graveyard contexts/ten roots. Verify
  actual root and linked-body collision callbacks, plus explicit shape0..6
  dependency traces with original poses/caches/room walls. Source callbacks
  may mutate geometry metadata/player/room flags; preserve those effects.
  Assert finite positions and stable original model/pose bindings. These
  dependency traces do not claim an ordinary joined gameplay encounter.
- [x] Run205room original scan, full68check build/package, fresh packaged
  dummy-SDL smoke, format/diff checks. One fresh source reviewer, material
  findings one RED/GREEN fix pass; minors recorded/deferred. Commit evidence.
  Root scheduling, armed combat, inventory/audio/saves and actual two-room
  acceptance remain mandatory unfinished Step1 work; New Game disabled.

## Evidence

Initial pending callback RED exits8. Source fixtures GREEN; added prior-cache-Y
fixture RED (Cached Ninja matrix outside float range) then GREEN after changing
the XZ-only source reads to ignore unused cached Y. The ordinary cached point
transformation paths retain complete native Ninja matrices. Source player.h
defines nlxb/nlyb/nlzb at EXP0+6c/70/74: separate PlayerExtendedWork normal_anchor
now preserves this contact fallback without reusing movement_anchor+48/4c/50.

Final focused4/4 checks pass: zombie-frame-collision,actor-contact-response,
stateful-wall-response,collision-line. Late head-cache rollback has a positive
control that proves player contact and primary wall response occurred first.
Original205room scan exits0:160callbacks/20originalroot-body/140posture0..6/
10roots/50displacedpositions, stable original pose/cache/model/motion/RNG.
All previous stair/grab/PlayerLink/corridor probes still pass.

Final full build/package exited0 with68/68 headless checks. Fresh packaged
dummy-SDL smoke exited0:241ticks/240renders/53moves/10focus-blocked ticks,
main-menu return and INI creation. Format/diff checks pass. Fresh source review
found no Critical/Important defects; its Minor asymmetric-posture fixture
recommendation is recorded and deferred. Ordinary encounter scheduling,
armed combat, inventory, audio, saves, traversal and reference fidelity are
outside this callback review and remain required before Step1 completion.
