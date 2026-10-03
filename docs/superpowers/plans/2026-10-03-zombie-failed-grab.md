# Native zombie failed-grab recovery implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development.

**Goal:** Port complete en01.c::NG01 failed-grab recovery with paired original
motions, wall/cliff recoil, source sound requests and atomic scalar updates.
**Architecture:** Add the callback beside NG00 in zombie_grab.cpp, sharing its
private validated Work/wall/cliff services. Synthetic tests assert original
numbers; original graveyard probes advance both works and source caches.
**Tech stack:** Windows x64, C++17, CMake/Ninja, SDL2; all verification headless.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:**69cce7a. Full Step1 remains active; this is a required combat dependency.

## Global constraints

- Native source code; no emulator or retail executable execution.
- User-owned USA ISO/assets remain private, ignored and absent from packages.
- All checks headless, fresh ignored diagnostic profiles; desktop untouched.
- Existing authorized codex/windows-foundation checkout and dependency runtime.
- NG02/head cutting/effects, PlayerLink, ordinary root routing, weapon combat,
  audio playback, inventory and versioned cross-room saves remain required.
- Source NG01 is complete here; do not replace unfinished callers with a stub.

## Review focus

- Source frame16 startup differs from NG00; same-motion retention applies.
- Recoil only on authored frames18..30; source double constants1.5/.14.
- Wall/cliff checks start after17; no NG00 cap/Z100/flag40 side effects.
- Ordinary recovery12/212 starts at frame7, flags40000; sound is frame20.
- Wall correction skips first yaw update at timer5, retains exact five steps.
- Cliff exit sets HP=-1 and paired {4,0,3,0}; ordinary exits {1,0,4,0}.
- Optional body accessed only when consumed; unknown states skip context.
- Scalar/queue/RNG changes roll back on late body/bank/cliff errors.

## Task1: source callback

Files: src/zombie_grab.cpp,src/zombie.hpp,
tests/zombie_failed_grab_tests.cpp,CMakeLists.txt.
Interface: tick_zombie_failed_grab(ScenarioState&,size_t,
const std::vector<int32_t>& playing={}) implements NG01 directly.

- [x] Write literal tests:7/207 frame16 blend5 startup/fallthrough; retained
  same-motion counters; recoil18..30 and .05 minimum; frame39 ->12/212 frame7;
  frame20 sound gate; ordinary stop flags; wall50/250 and five corrections;
  cliff124/324/death; null body; unknown/missing unused context; rollback.
- [x] Register pending callback, build and targeted test.
  Expected:FAIL with native failed-grab recovery pending.
- [x] Implement complete source states0..4 using existing native services.
  Expected:targeted PASS and existing NG00/player-grab tests remain passing.

## Task2: original paired sequence and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] All10original graveyard roots: full NG01 startup ->authored recoil ->
  recovery ->walking or authored cliff death, paired SetMtn/CalcEnemy at each
  unfinished tick. Expected:real7/207 then12/212 or50/250 or124/324 according
  to actual collision, paired stop modes/flags and finite caches;
  all previous205-room probes retained. This does not prove full root routing.
- [x] One fresh read-only final source review; material findings one RED/GREEN
  fix pass plus fresh full suite; Minor findings recorded/deferred.
- [x] tools/build.ps1. Expected:57/57checks, updated EXE/ZIP. Final original
  runtime scan and fresh packaged dummy-SDL smoke. Expected:205rooms, new
  failed-grab probes, prior probes retained;241ticks/240renders/53moves/
  10focus-blocked ticks/menu return/INI creation.
- [x] Format/diff checks, evidence/rulings ledger, status/plan update, commit.
  Full Step1 acceptance remains unproven and goal stays active.

Verified:57/57checks,205rooms,436NG01states/852paired samples/10recoveries,
fresh packaged smoke241ticks/240renders/53moves/10blocked/menu/INI creation.
Initial core recovery permission failure and independent/full passing reruns
are recorded; deterministic sharing-lock handling is a separate next increment.
Review no material findings; missing-bank rollback coverage deferred.
