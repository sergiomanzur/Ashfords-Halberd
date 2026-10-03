# Native actor contacts implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, with test-first changes and one final source review.

**Goal:** Port the original enemy/player contact response needed by the graveyard encounter and connect baseline player contacts before wall response.

**Architecture:** Portable contact bodies represent positions, collision offsets, dimensions, flags and stable parent indices. Pair and pool response stage mutations before commit. Locomotion accepts an optional native contact context, preserving the existing pre-contact front probe and applying contact displacement before both wall passes. Runtime converts original enemy work into contact bodies and commits touched flags after successful movement.

**Tech Stack:** C++17, CMake/Ninja, native math, headless CTest and private retail runtime scan.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64, pinned MIT upstream source, immutable retail assets.
- All checks headless; fresh ignored diagnostic profiles only.
- Full zombie movement/damage dispatch and remaining Step 1 acceptance stay required; New Game stays disabled.
- Use original atan-to-word truncation and Ninja table for one-sided response.

## Review focus

- Height intervals are inclusive; horizontal tangency is excluded; floor numbers do not gate this source function.
- Player contact gates differ from enemy pool gates; linked hidden parents suppress pool collision.
- Multiple pool contacts average candidates computed from the original subject position rather than iterating displacement.
- Coincident pair normalization retains a zero vector; one-sided response uses original angle zero.
- Late invalid parent or nonfinite arithmetic cannot partially commit flags or positions.

## Task 1: Original pair and pool contact response

**Files:** Create `src/actor_contacts.hpp/.cpp`, `tests/actor_contacts_tests.cpp`; update `CMakeLists.txt`.

**Interfaces:** `ContactBody` owns position/offset/normal_anchor, radius/height, primary/state/secondary flags, optional linked-parent slot. `bool collide_enemy_player(ContactBody&, ContactBody&)`; `size_t collide_actor_pool(ContactBody&, vector<ContactBody>&, optional<size_t> subject_slot={})`. `ActorContacts` holds player and enemy bodies for locomotion.

- [x] Test a pair at z=0 and z=3 with radii 2: split response moves enemy to -0.5 and player to 3.5; fixed player leaves player at 3 and pushes enemy near -1. Check gates, offsets, fallback normal anchor, height boundaries, coincidence, flags and malformed rollback.
- [x] Test player at z=0 between eligible enemies z=1 and z=-2 with radius sums 4: original candidates -3 and 2 average near -0.5. Check pool order, self exclusion, linked-parent hidden/dead flags, player-only flag0x40, pool limit and late parent failure.
- [x] Build unavailable implementations and observe an expected failing contact executable. Implement source `hitchk.c::bhCheckPlayer/bhCheckEnemies` response and pass the targeted test.

## Task 2: Native player frame integration

**Files:** Modify `src/locomotion.hpp/.cpp`, `src/scenario.hpp`, `src/runtime.cpp`; extend `tests/locomotion_tests.cpp` and retail inspector; update `docs/player-movement-status.md`.

- [x] Extend `advance_player_world` with optional `ActorContacts*`. Write a failing world check that an eligible enemy displaces the player and receives state flag4, while the front probe retains pre-contact position. Verify late wall failure preserves the passed contact context.
- [x] Call pool collision after front-probe calculation and before wall checks, as `player.c` does. Use contact dimensions 2.5/15.5 for the selected Claire baseline; retain existing wall dimensions 3.5/16.5. Add native collision offsets and player secondary flags to runtime state; preserve original normal-entry flag mask.
- [x] Exercise contact response with initialized retail zombie geometry; run full build, retail scan and packaged smoke, then source review. Resolve important findings, document represented behavior and commit.

## Source evidence

`hitchk.c` bhCheckPlayer/bhCheckEnemies; `player.c` bhInitRoomChangePlayer, PlyInfo and player frame order; `ps2_NaMatrix.c` njUnitVector zero-vector behavior; `ps2_NaMath.c` njSinCos table. Full zombie controller remains separate.
