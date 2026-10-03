# Native zombie model/collision refresh plan

> Execute inline with superpowers:executing-plans; use one final source review.

**Goal:** Port all branches of en01.c::bhEne01_CalcEnemy and use them after
native SetMtn, replacing diagnostic full-cache refreshes with source timing.
**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Architecture:** Stage only model-work cache vectors, work-specific base
matrices, capsules/heights and radius flags. Preserve shared ML_WORK ownership
through stable indices. Root ordinary work reads its prior cached joint10;
linked body work calculates the parent; detached body work calculates the
parent followed by its own joint8 suffix, restoring the full range afterward.
**Technology:** Native C++17/CMake/CTest, GCC, existing Ninja float matrices.

## Constraints / review focus

- Headless checks only; retail data stays private and ignored.
- EXP80/84 and EXP88/8c are distinct limb damage fields; the capsule's exact
  -1 gate reads88/8c. Prior native startup used the wrong fields.
- Primary80 takes priority over EXP80000000. Root work must not gain a fresh
  cache calculation or touch unused model, pose, parent or base-matrix inputs.
- Detached suffix results share the actual owner's O_WORK vector when source
  ML_WORK aliases; parent and child retain separate BH_PWORK base matrices.
- Preserve frozen O_WORK flags, source float capsule transforms/averaging and
  original negative height values. Radius200 grows by.2 and clamps at2.5 only
  when active; unconsumed invalid radius/base/pose inputs stay ignored.
- All consumed validation/calculation finishes before committing caches,
  capsules, scalar flags or per-work matrices. Models/scenes remain untouched.
- Drawing, npCalcMorphing, effects, full root AI/combat, decoded audio, inventory
  and cross-room saves still require their own integration. No New Game enable.

## Task1: source callback and startup correction

Files: new `src/zombie_model.cpp`, `src/zombie.hpp`, `src/scenario.hpp`,
`src/zombie.cpp`, `src/zombie_motion_table.cpp`,
new `tests/zombie_model_tests.cpp`, existing zombie startup/table tests,
`CMakeLists.txt`.
Interface: `void calculate_zombie_model(ScenarioState&, size_t)`.
Rename prior misnamed fields to source offsets `damage_80/84`, add88/8c, and
retain a separate optional `model_matrix` per work for source1000 base reuse.

- [x] Write the pending callback and source literal regressions. Observe RED:
  `cmake --build build/windows --target zombie-model-tests zombie-tests --parallel 4`
  then `ctest --test-dir build/windows -R 'zombie-model|zombie-parameters' --output-on-failure`.
  Test root stale-cache timing, attached normal/trunk capsules, exact leg -1
  versus negative arm/-2 states, detached shared suffix/prefix and separate
  matrices, frozen joints, linked priority,1000 reuse, radius flags/clamp,
  negative height, ignored unused context and late-error atomicity. Existing
  startup regression proves the wrong damage-offset check before correction.
- [x] Implement all source branches with staged cache updates. Use generic
  `calculate_actor_model_work`, source Ninja point transforms and literal float
  averaging. Keep parent/child actor bases independent from shared cache range.
  Startup delegates its capsule calculation to this source callback and keeps
  its existing explicit rendering step after calculation.
- [x] Run targeted GREEN and existing foot-table/SetMtn/startup checks.

## Task2: actual retail cache timing

Files: `tools/room_inspect.cpp`, `docs/scenario-status.md`, this plan.

- [x] After root SetMtn call calculate root; after body SetMtn calculate body.
  Preserve source's cached root/neck inputs and run next frame's paired motion.
  Check refreshed parent capsules/body heights, exact unchanged-pose fix paths,
  and derive posed rendering from the resulting cache without altering assets.
- [x] Final source review against en01.c3102â€“3195/MdlPut.c240â€“309. Fix any
  Critical/Important finding once with a failing regression then passing suite.
- [x] Run retail205-room scan, `./tools/build.ps1`, fresh packaged dummy-SDL
  smoke and diff/format checks. Commit progress; keep complete Step1 active.
