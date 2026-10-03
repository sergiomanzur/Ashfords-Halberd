# Native zombie pursuit and stair reservations implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, test-first, followed by one source review.

**Goal:** Adapt Brain00 pursuit and its original stair-query/reservation dependencies for subsequent root movement dispatch.

**Architecture:** Keep route targets/timers in extended work. Stair ownership uses stable indices into current-room static and dynamic miscellaneous records, replacing PS2 pointers. Stage scalar pursuit changes and validate both stair records before committing any state; retain source byte wrapping and query order.

**Tech Stack:** C++17, CMake/Ninja, headless synthetic and private retail checks.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Windows x64 native code from pinned MIT decompilation; no retail assets committed or packaged.
- All verification headless with ignored diagnostic profiles; no normal user-data writes.
- This implements controller dependencies; root movement, combat, audio and cross-room saves remain required for Step 1. New Game remains disabled.

## Review focus

- Stair look-behind uses original Ninja sine/cosine; inclusive XZ probe, half-unit alignment inset, source first-match and static-before-dynamic ordering. Y is ignored.
- Stair pairing stays inside the source array for adjacent prm3=255; explicit prm3 always addresses static records, including a dynamic first record.
- Retain actor reservation indices after clearing, matching uncleared source pointers; failed index validation cannot change either attribute.
- Brain updates awareness first logically, but errors roll back awareness, targets, route timers, modes and reservation attributes together without copying scene geometry.
- A shared route region legitimately returns255 and writes destination. Missing region leaves source local output uninitialized; reject that unsupported path instead of inventing a destination.

## Task 1: Stair query and ownership

**Files:** new `src/enemy_navigation.hpp/.cpp`, `src/scenario.hpp`, `src/runtime.cpp`, new `tests/enemy_navigation_tests.cpp`, `tests/room_tests.cpp`, `CMakeLists.txt`.

**Interfaces:**

```cpp
std::optional<size_t> enemy_stair_trigger(const RoomCollision&, const std::vector<CollisionRecord>&, Vec3, int32_t yaw, int32_t floor, float offset);
void reserve_actor_stairs(ScenarioState&, size_t actor, size_t index);
void release_actor_stairs(ScenarioState&, size_t actor);
```

- [x] Write fixtures with static/dynamic stairs and literal expected indices, edge/inset/orientation, occupied/inactive/wrong-floor filtering, ignored Y and source probe-overwrite behavior. Add paired reservation/release and malformed-index rollback tests, run pending stubs and observe RED.
- [x] Implement `bhKaidanAtrCheck`, `bhSetUseKaidanFlag`, `bhClrUseKaidanFlag`; keep pair bounds local to the original vector. Run `ctest --test-dir build/windows -R '^enemy-stair-reservations$' --output-on-failure` and observe GREEN.
- [x] Pin initializer stair release and extended-work reset; observe RED before adding the source release call. Pin current-room dynamic miscellaneous clearing through the production room loader while preserving caller state, observe RED then GREEN; new rooms rebuild actor indices through existing work allocation.

## Task 2: Brain00

**Files:** new `src/zombie_brain.cpp`, internal `src/zombie_awareness.hpp`, `src/zombie_awareness.cpp`, `src/zombie.hpp`, `src/scenario.hpp`, new `tests/zombie_brain_tests.cpp`, `tools/room_inspect.cpp`, `docs/scenario-status.md`, `CMakeLists.txt`.

**Interface:** `void tick_zombie_brain(ScenarioState&, size_t actor, const ScenarioEntity& player)`.

```cpp
// EXP0 fields: route_id/timer=0x2a/0x2b, target_x/z=0x58/0x60,
// path_blocked=nonzero0x70, stair_target=0x74.
if (type != 5 && noticed && distance < 50 && !path_blocked && same_floor)
  // target player XZ, current route ID, timer0
else if (type == 5 || (noticed && (cached_id == current_id || ++timer >= 255)))
  // source destination-row/source-column route query
// Then original player-state/other-zombie/stair-capacity/floor-direction gates.
// Successful stair: target/reserve pair; mode1=0,mode2=8,mode3=0 and flag400000.
// Primary200000 instead clears notice400 and releases pair.
```

- [x] Test awareness integration, direct pursuit, exact50 threshold, obstruction and different floors, route cache/time254->255 refresh and255->0 wrap, same-region255 output, type5 unconditional routing, no-notice preservation, missing/malformed routes and rollback. Observe pending stub RED.
- [x] Refactor awareness into a const evaluated scalar result consumed by both wrappers; implement Brain00 including original stair gates and validated staged commits. Test player flags4/status80000000, eligible competing root400000 and source exclusions, prm2<4, first-match low capacity, uphill/downhill and primary200000 release. Observe GREEN.
- [x] Add isolated Brain00 traces using actual graveyard cached model work and valid close pursuit targets, plus authored stair query probes; do not count these as full AI frames.
- [x] Run targeted tests, source review, complete Windows build/CTest/install/ZIP, fresh retail inspector and packaged headless smoke. Check diff/format, record limits and commit the increment.

## Source evidence

`zonzon.c::bhKaidanAtrCheck`, `hitchk.c::bhSetUseKaidanFlag/bhClrUseKaidanFlag`, `en01.c::bhEne01_Brain00/bhEne01_KaidanCheck`, `rutchk.c::bhCheckRoute`; source-owned array adjacency and the query's overwritten px/pz temporaries must remain visible in tests.

## Verification result

Targeted five-test suite passed. Source reviewer approved with no findings.
The private retail scan passed205 rooms, including146 authored stair queries
and930 isolated close-pursuit updates across ten original roots. The existing
85,944 route queries and1,860 awareness updates remained passing. These are
controller probes; the player primary4 gate isolates close pursuit from stairs.

Full tools/build.ps1 passed31/31 CTests, installed the executable and rebuilt
the ZIP. Fresh packaged headless runtime passed241 ticks,240 rendered frames,
53 movement ticks, ten focus-blocked ticks, main-menu return and INI persistence.
Diff and modified production/new-test formatting checks passed.

Test expectations corrected from source before final verification: quarter-wave
probe success still requires the actual actor alignment inset; a stored low31
awareness byte increments into pending0x20; filtering an occupied first stair
allows a second eligible candidate, so the no-stair fixture reserves both.
Implementation follows source rather than those initial expectations.

Step1 remains active. Root movement/action/motion dispatch, damage/attacks and
player response remain the next encounter work, followed by inventory, audio
and versioned save/load across the selected connected rooms.
