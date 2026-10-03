# Zombie capture retry and authored turn implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development;
> one fresh read-only review of the complete increment at the final gate.

**Goal:** Port complete en01.c::MV06 and MV07 source callbacks needed by
ordinary zombie encounter dispatch.
**Architecture:** Stage root/body motion and touched scalar fields. MV06 uses
the existing ikou3 and EatCheck with a scalar yaw override referencing the
original cache owner; MV07 stores its source ayp increment in yaw_correction.
No actors/models/poses are copied per callback. Keep ordinary root scheduling
disabled until its other required source callbacks are complete.
**Tech Stack:** C++17, CMake/CTest, pinned MIT source, Windows x64.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:** e961367, clean;64checks/205rooms/packaged smoke verified.

## Global constraints

- 100% native Windows x64; user-supplied USA assets remain private/ignored.
- All verification headless; dummy SDL and separate fresh diagnostic profiles.
- Preserve fixed simulation ticks and source state/flag/motion semantics.
- Never satisfy required gameplay services with silent no-ops.
- Full Step1 includes encounter/combat/inventory/audio/save-load; remains active.

## Review focus

- Same incoming motion: ChgMtn resets speed but skips frame/blend/mode reset.
- Failed late steering/capture/body validation: no RNG/player/pair partial commit.
- Linked-primary80 and suppressed capture: unused coordinates/cache stay unread.
- Turn delta32768 vs32769, negative division and signed32yaw overflow.
- Frame16/17/39/40/41: apply source turn window and exact return frame only.

## Task1: complete source callbacks and literal fixtures

Files: src/zombie.hpp,new src/zombie_retry_turn.cpp,
new src/zombie_attack_internal.hpp,src/zombie_attack.cpp,
new tests/zombie_retry_turn_tests.cpp,CMakeLists.txt.

Interfaces produced:
```cpp
void tick_zombie_capture_retry(ScenarioState&,size_t,ScenarioEntity&);
void tick_zombie_turn(ScenarioState&,size_t,const ScenarioEntity&);
bool detail::zombie_capture_yaw(const ScenarioState&,size_t,
    const ScenarioEntity&,int32_t yaw,int32_t cone,float distance,int mode);
```
Consumes enemy_turn_step(enemy,player.position,910), enemy_direction and the
existing source EatCheck/cache resolution, detail::MotionState/change/commit.

- [x] Add pending implementations and fixtures, register two CTests. Observe
  FAIL with Native MV06/MV07 pending. Literal startup assertion:
  ```cpp
  p.flags=5;
  tick_zombie_capture_retry(s,0,p);
  ck(s.enemies[0].motion_number==49 && s.enemies[0].frame==2949120 &&
     s.enemies[1].motion_number==249 && s.random_seed==1103527590u,
     "Source retry pair/RNG startup");
  ```
- [x] Implement both complete switch callbacks. MV06 startup49/249,45frame,
  blend15,flag40000,one15..29 RNG timer,mode1=1; fallthrough910steer; clear
  player4 admits EatCheck4551/5.5/mode0 using changed yaw; capture packed root/
  body modes2,0,0,0 +player10004 +z80; miss0/5/0; blocked player4 remains.
  MV07 startup127/327,blend8,clearz3000000; uint16target-yaw divided22 with
  delta<=32768 positive,else negative and bothmotionmode2;flag40000; fallthrough
  frames17..39yaw+=ayp;frame40mode1/2/3=1/1/0. No body mode copy on return.
- [x] Run both focused tests and existing lunge/capture/movement tests GREEN.
  Include linked/unlinked/same-motion paths, packed state preservation, unused
  data, strict cone/radius/Y/health gates, frame truncation and late failures.

## Task2: original assets and final gate

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Extend original205room probe: ten initialized roots; original49/249
  paired retry startup and native sampling; cached Claire capture followed by
  complete NG00/PlyDG00 handshake; both directions127/327 complete turn until
  authored40; paired source SetMtn/CalcEnemy on every continuing tick. Preserve
  all prior grab/PlayerLink/corridor probes and raw retail model identity.
- [x] Full66check headless build/package,205room original scan, fresh packaged
  dummy-SDL smoke. Format/diff checks; one fresh reviewer, material findings
  one RED/GREEN fix pass, minors recorded/deferred. Commit verified increment.
  Complete native playable slice remains active; New Game remains disabled.

## Evidence

Both pending callbacks failed with their named MV06/MV07 messages before
implementation. Focused5/5 source/movement/lunge/capture checks then passed;
same-motion turn fallthrough and retry signed32bit wrap additions pass2/2.
An unlinked turn fixture accidentally kept Z-4 while expecting an X4/Z0
quarter turn; its position was corrected, production direction was unchanged.

Original205room scan exits0: ten initialized roots,820MV07states/1600paired
samples/20authored40returns;20MV06states/10cachedClaire captures/20NG00-PlyDG00
handoff ticks/60paired enemy samples/10original407player samples. All previous
PlayerLink/grab/step/corridor probes remain passing. These complete callbacks
and joined dependencies; ordinary root encounter scheduling remains separate.

Final verification: build.ps1 exits0 with66/66checks and refreshed EXE/ZIP,
no compiler warnings. Fresh packaged dummy-SDL smoke exits0 with241ticks/
240renders/53moves/10focus-blocked ticks/main-menu return/INI creation. Current
original205room scan exits0; report206lines including header. Format/diff
checks pass. All build/scan/smoke handles are terminal before commit.

One fresh source reviewer found no Critical/Important/Minor defects. Reviewer
set aside full encounter/combat/inventory/audio/save completion and execution
of independent gates: the former remains unfinished Step1 work with New Game
disabled; the latter has now passed under headless verification. No fixes or
second review were required. Full native playable slice remains active.
