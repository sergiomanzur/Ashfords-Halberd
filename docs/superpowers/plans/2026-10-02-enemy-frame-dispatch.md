# Native auxiliary enemy frame dispatch plan

> **For agentic workers:** Use superpowers:executing-plans inline with test-first changes and one source review.

**Goal:** Complete original head/arm/leg idle and parent-manager frame dispatch, plus common post-controller damage cleanup, required by the graveyard zombie scheduler.

**Architecture:** Dispatch stable pool indices in original order through the original shared callback registrations. Existing auxiliary startup owns initialization; manager updates stage only scalar root flags rather than copying full scenes. Root work retains the original cached player distance. Common post-controller cleanup resolves ML_WORK ownership and clears only its source object range, combo transient bits, timeout and flg2 damage bit.

**Tech Stack:** C++17, CMake/Ninja, headless synthetic and retail runtime checks.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64 using pinned MIT source; retail assets remain private and immutable.
- All verification headless and normal user profile untouched.
- Full root zombie movement/damage/attack dispatch remains required for actual graveyard travel; do not enable a static encounter or New Game.
- Preserve source stage-OR-room stair matching and first-in-pool distance ties.

## Review focus

- Only active, visible, unlinked, initialized ID1 roots of type other than10 qualify for manager policy.
- Read cached EXP0 distance84; do not replace it with freshly computed distance.
- Clear nearest/stair bits only on eligible roots; excluded work retains those fields.
- The original stair scratch has ten slots; reject an overflow transactionally.
- Damage cleanup follows each successful callback only, respects aliased ML_WORK object count and leaves status flags and unselected damage entries intact.
- Shared callback IDs remain unassigned until source startup registers their specific function; unsupported registrations fail explicitly.

## Task 1: Auxiliary and manager dispatch

**Files:** Modify `src/zombie.hpp/.cpp`, `src/scenario.hpp/.cpp`, `CMakeLists.txt`; create `tests/enemy_scheduler_tests.cpp`.

**Interfaces:** Add `float ZombieExtendedWork::player_distance`; `void tick_zombie_auxiliary(ScenarioState&,size_t)` dispatches IDs31/32/33/38 modes0/1, rejects unsupported damage states.

- [x] Write a production scheduler test with manager slot0 and root slots1â€“3 distances30/40/10; verify root3 gets nearest flag2, two floor2 roots inhibit root3 stairs, and equal distance preserves first slot. Gate root callbacks with status40000000 for isolated policy testing; source policy does not exclude that bit.
- [x] Check excluded type10, hidden, linked, disabled and missing extended work; nonmatching stage1/room5 preserves stair bits, stage0 or room6 independently enables the source rule. Check fewer than two floor2 roots clears eligible stair bits.
- [x] Exercise original auxiliary mode0 startup and mode1 empty source callbacks through the production scheduler. Test hidden-parent suppression, invalid parent, unsupported damage state, invalid cached distance and scratch overflow rollback.
- [x] Observe failure on the unsupported manager scheduler; implement scalar staged manager updates and indexed production dispatch, then pass the targeted suite.

## Task 2: Common cleanup and retail integration

**Files:** Modify `src/scenario.hpp/.cpp`, `tests/enemy_scheduler_tests.cpp`, `tools/room_inspect.cpp`, `docs/scenario-status.md`.

**Interfaces:** Add `int32_t ScenarioEntity::combo_timeout`; internal `finish_enemy_frame` adapts `zonzon1.c::bhEne_InitDamage`, resolving model owner by stable index.

- [x] Test a held ID67 with three model joints, 64 damage entries set99, timeout2, combo_flagsff and secondary_flagsf: cleanup clears only first3 entries, timeout becomes1, combo_flags8f, secondary_flagsb, and status flags remain unchanged. Test null model, held ID91 prepared-joint count, aliased auxiliary ownership and negative timeout wrap.
- [x] Verify cleanup executes after success for startup, held and moving represented controllers; unsupported callbacks must fail before cleanup.
- [x] Exercise both retail graveyard variants' auxiliary scheduler and manager policy with initialized real work, without claiming the pending root controller runs. Run full build, private retail runtime scan, packaged smoke with a fresh ignored profile, source review and diff/format checks; document limits and commit.

## Source evidence

`eneset.c::bhControlEnemy`; `en01sub.c` head/arm/leg/parent dispatch, empty MV00 callbacks, parent nearest/stair policy; `zonzon1.c::bhEne_InitDamage`. Root zombie mode dispatch remains a separate required implementation.

## Verification outcome

Full `./tools/build.ps1` passed all 26 CTests and rebuilt the installed EXE/ZIP. The fresh ignored `local/enemy-frame-dispatch-runtime-ui-1` packaged smoke passed 241 ticks, 240 rendered frames, 53 movement ticks, ten focus blocks and return to menu. The real retail scan passed 205 rooms, 3,328 isolated auxiliary scheduler frames and 126 manager updates; original root AI remains explicitly gated in that diagnostic. Source review approved this scoped dispatch/cleanup increment.
