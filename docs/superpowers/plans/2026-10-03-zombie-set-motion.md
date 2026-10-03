# Native zombie SetMtn implementation plan

> Execute inline with superpowers:executing-plans; use one final source review.

**Goal:** Port en01.c::bhEne01_SetMtn orchestration using existing native motion
sampling, displacement, speed and foot-table routines.
**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Architecture:** Share existing validated channel application without copying
the entire runtime. Retain source byte recoil counter and float mouth scalar.
Require explicit synchronous KamiEffect/WaterEffect callbacks for mode0<5;
they run before mouth/table processing and receive the captured incoming frame.
They are dependency boundaries, not an implemented effects backend. Validation
uses source-proven dry walking callbacks; gameplay cannot silently omit effects.
**Tech stack:** Native C++17, GCC/SDL2, CMake/CTest and Python integrity check.

## Constraints and review focus

- All checks are headless with private retail data and fresh ignored profiles.
- Frozen EXP10 bypasses unused motion/owner/effect inputs and returns0.
- Linked bodies sample into owner joint8, preserve separate frame counters and
  ignore root displacement flag1000000; body speed flag2000000 uses AddSpeed.
- Callback and mouth/table timing uses the frame captured before bhSetMotion.
- Disabled mouth flags and type10 preserve stale mdflg2/shp_ct as source does.
- Failed sampling or a consumed invalid table index restores the current pose,
  motion scalars, parent recoil state, RNG and appended sound/vibration queues.
  Hooks may append requests/update RNG; backend storage owns its own rollback.
- Full source effect implementations, CalcEnemy morphology and root AI/combat
  are still required. This increment does not enable New Game or root dispatch.

## Task1: source callback and retained data

Files: `src/zombie_set_motion.cpp`, `src/zombie_set_motion_tables.hpp`,
`src/zombie_motion_internal.hpp`, `src/zombie.cpp`, `src/zombie.hpp`,
`src/scenario.hpp`, `tests/zombie_set_motion_tests.cpp`,
`tests/zombie_motion_table_data.py`, `CMakeLists.txt`.

Interface: `int tick_zombie_motion(ScenarioState&, size_t,
const ZombieMotionEffects&, const std::vector<int32_t>& playing={})`.
`ZombieMotionEffects` has synchronous `kami` and `water` functions accepting
`(ScenarioState&, size_t, int32_t captured_frame)`.
Share `detail::apply_zombie_motion_channels(ScenarioState&,size_t)`; existing
startup and raw diagnostic application retain their behavior.

- [x] Write tests and a pending implementation; observe RED using
  `cmake --build build/windows --target zombie-set-motion-tests --parallel 4`
  and `ctest --test-dir build/windows -R zombie-set-motion --output-on-failure`.
  Pin root translation/priority/zeroing, frame boundary, incoming-frame hooks,
  disabled hooks, shared body joint8, speed/turn gates, all recoil combinations
  and eighth-byte reset, all mouth tables/type10/flag-off state, exact callback
  ordering/RNG and foot fix, null records/frozen inputs and failure rollback.
- [x] Implement source SetMtn flow, requiring initialized owner pose/cache.
  Snapshot only consumed pose/scalars, preserving immutable models and scenes.
  Original mouth tables71/70/40 and rot_tbl4x8 are native public MIT constants.
  Extend source verifier to compare all mouth values and recoil integers.
- [x] Run targeted GREEN and existing zombie/channel tests; verify data checker.

## Task2: retail integration and checkpoint

Files: `tools/room_inspect.cpp`, `docs/scenario-status.md`, this plan.

- [x] Replace the isolated walking raw sampler/table sequence with paired source
  SetMtn calls. Effect probes reject wet work and require selected walking
  motions, whose source Kami table has no entries. Recompute cached model work
  after both calls; retain exact zero-drift and existing event checks.
- [x] Review against pinned en01.c::SetMtn and Motion.c::bhSetMotion. Correct
  Critical/Important findings in one RED/GREEN fix pass.
- [x] Run retail205-room scan, full `./tools/build.ps1`, fresh packaged dummy-SDL
  smoke, diff/format checks. Commit verified progress and keep Step1 active.

## Verification evidence

Observed RED for pending SetMtn and empty mouth constants, then targeted GREEN.
An inherited O_WORK flag omission reproduced with protected root/body angles;
forwarding cached flags passed that regression and the Normal skip cursor test.
Source integrity passes1,008 prior table integers,181 mouth scalars and32 recoil
integers. One final read-only source review found no unresolved issues.
The final retail scan passes205 rooms,1,600 paired SetMtn calls,3,200 proven dry
effect callbacks and4,800 zero-drift foot paths. Full Windows build/40 CTest
checks/package pass. Fresh packaged dummy-SDL smoke passes241 ticks/240 renders,
53 movement ticks/10 focus blocks/menu return and INI persistence. No GUI used.
Step1 remains active: actual effects, full enemy/combat dispatch, decoded audio,
inventory interaction and cross-room saves require further implementation.
