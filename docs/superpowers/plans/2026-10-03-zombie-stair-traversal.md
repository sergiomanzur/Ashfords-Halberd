# Complete zombie stair traversal implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development;
> one fresh read-only review of this complete increment at its final gate.

**Goal:** Complete en01.c::MV08 so reserved stairs can be traversed uphill
and downhill using authored paired clips, heights, floors and source cleanup.
**Architecture:** Stage touched root/body motion and scalars, signed stair
bytes, owner root-pose Y and reservation pair pointers. Consume only fields
each source branch reads; no per-tick model/pose/actor copies. Existing stair
selection/reservation, floor lookup, walking pairs and SetMtn are dependencies.
**Tech Stack:** C++17/CMake/CTest, pinned MIT source, Windows x64.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:** e4908aa clean,66checks/205rooms/fresh packaged smoke verified.

## Global constraints and review focus

- 100% native Windows x64; private user-supplied USA assets stay ignored.
- Headless only, dummy SDL and fresh ignored diagnostic profiles.
- Preserve fixed ticks, source ChgMtn shortcuts, float/byte/word arithmetic.
- No silent replacement of required source services; Step1 remains active.
- Signed stair bytes wrap while prm2 height/count input remains unsigned.
- Repeated frame40 increments, root-only repeat frame reset, exact ct1!=0.
- Completion copies previous position before floor snap, resets owner pose Y,
  releases the saved kdnp pair and retains both target/reservation indices.
- Late malformed ground/cache/owner pose/pair rejects all staged changes.
- Inactive stored targets remain readable; unused playerY/geometry/cache ignored.

## Task1: complete MV08 callback and literal source fixtures

Files: src/scenario.hpp,src/zombie.hpp,new src/zombie_stairs.cpp,
new tests/zombie_stairs_tests.cpp,CMakeLists.txt.
Produces:
```cpp
void tick_zombie_stairs(ScenarioState&,size_t,const ScenarioEntity&);
// ZombieExtendedWork: int8_t stair_start_floor,stair_step (EXP0_C2d/2e).
// ScenarioEntity: int32_t pitch_correction (axp),float floor_target_height (yn).
```
Consumes detail::MotionState/change/commit, zombie_walk_motion,
player_floor_number, detail::stair_pair; source cache owner and root joint pose.

- [x] Add pending callback and CTest; observe FAIL Native MV08 pending.
  Literal startup fixture:
  ```cpp
  tick_zombie_stairs(s,0,p);
  ck(e.motion_number==2 && b.motion_number==202 && e.control0==9 &&
     e.angles[1]==7776 && e.floor_target_height==16,
     "MV08 source startup/fallthrough");
  ```
- [x] Port all states0..5: source2/202 blend10 startup, unsigned stair input,
  target yaw [0,49152,32768,16384], signed delta/10 and ten decremented turns;
  source58/59 entry,56/57 repeat,60/61 exit with paired200offset; cached root Y
  constants1.493898/.880405; signed stair-step byte increments/decrements;
  repeated exact40 increment and ct1-=2/root-only frame1 reset until zero;
  terminal ground lookup, walking frame50/blend15, owner poseY0 and saved pair
  release; complete alternate state5. Stage all commits after late validation.
- [x] Source fixtures GREEN alongside existing navigation/brain/motion tests:
  both directions/four yaw constants/half-turn and signed32wrap, same-motion
  shortcut, signed byte wrap vs unsigned input, repeat and exact frame gates,
  static/dynamic stored targets and reservation pairs, distinct cache owner,
  previous Y before snap, paused/unused branches and late failure rollback.

## Task2: original paired asset paths and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Collect one original uphill and downhill stair pair with valid direction
  and even prm2>=4 across the205room scan. Retain both actual graveyard
  initialized model/motion contexts (ten roots total). Join each root to both original
  stair records and original room ground; reserve then complete MV08 with
  paired SetMtn/CalcEnemy until walking handoff. Verify floor/height, byte
  counts, release, owner pose reset and unchanged raw model. These are shared
  asset dependency probes, not an ordinary gameplay encounter claim.
- [x] Full67check headless build/package,205room original scan, fresh packaged
  dummy-SDL smoke. Format/diff checks, one fresh read-only source review;
  material findings one RED/GREEN fix pass, minors recorded/deferred. Commit
  verified increment. Full playable slice remains active/New Game disabled.

## Evidence

Pending MV08 failed before implementation. Focused source/navigation/brain/
displacement checks pass4/4; additional signed-word/exact-frame/owner-cache/
late-body rollback fixtures pass1/1. Original205room scan exits0 and completes
3270MV08 updates,6500paired authored samples,20uphill/downhill handoffs across
ten initialized roots using both original stair directions from room7.

The first original probe expected2/202 unconditionally. Instrumentation proved
incoming motion2/blend0 immediately becomes58/258/state2: original ChgMtn
retains blend_count for the same motion, and MV08 falls through case0->1.
Only the diagnostic expectation changed; production remained source exact.
These are joined asset dependency probes, not a playable enemy encounter.

Final build.ps1 exits0:67/67 headless checks and refreshed EXE/ZIP; no compiler
warnings. Fresh packaged dummy-SDL smoke exits0:241ticks/240renders/53moves/
10focus-blocked ticks/main-menu return/INI creation. All subprocesses terminal.
Source review found no Critical/Important/Minor defects. Parent verified the
independent original scan/build/package/smoke gates; unselected malformed ground
remains ignored as source comparisons dictate (retail decoding rejects it).
GUI/audible verification is replaced by future headless rendered/decoded-audio
acceptance under the user's requirement. Ordinary encounter, inventory/audio/
saves and the selected full two-room playthrough remain mandatory unfinished
Step1 work. No second review or review fix pass was needed.
