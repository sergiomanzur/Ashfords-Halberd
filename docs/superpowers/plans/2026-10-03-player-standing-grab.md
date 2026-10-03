# Native player standing-grab controller

Spec: docs/superpowers/specs/2026-10-02-windows-port-design.md.
Base:9aa6e1d. Execute inline in the authorized Windows checkout; headless only.

Port complete en01.c::PlyDG01 states0..5 and route PlayerControl mode2=1.
This removes the standing-player callback dependency without claiming NG02,
head-cut effects, PlayerLink, root dispatch or full combat are implemented.
Original character offsets400/425/450/425 and relative clips1/2/4/5 apply.
Release uses cached player root XZ, writes Player EXP0 movement anchor XZ,
clears source input/flags, restores base bank, enters motion42 and performs
the immediate source bhSetMotion sample with blend_count8 retained.

## Constraints and source decisions

- Original sys->ply_id0 is Claire; only Claire front-facing held pose adds
  frame*546 to ayp. State4 does not increment ct0, unlike state1 frame11.
- State2 does not fall through to state3. Release frame0 reads preexisting
  cached root XZ before sampling base motion42; Y remains unchanged.
- Direct source bhSetMotion does not decrement hokan_count or rebuild cache.
- Stage only scalars except actual release pose sampling. Reserve queues
  before committing; invalid bank/cache/pose context must leave state unchanged.
- Source status EXP1 remains distinct from movement anchor EXP0.
- Unknown states and disabled PlayerControl gates consume no extra context.
- Existing retained matrix, original clip data, and native motion helpers
  are used; source PlayerLink/world positioning remains a separate dependency.

## Task1: complete player callback

Files: src/player_standing_grab.cpp,src/player_grab.cpp,src/zombie.hpp,
src/scenario.hpp,tests/player_standing_grab_tests.cpp,CMakeLists.txt.
Interface: tick_zombie_player_standing_grab(ScenarioState&,size_t,
ScenarioEntity&) implements the direct callback; existing player controller
routes mode2=1 after the original active-grab/player-mode gate.

- [x] Tests first: state0->1 fallthrough; frame11 character/facing clips;
  frame19 voice/suppression; Claire ayp frame546 arithmetic; state2 release
  setup; state3 frame0 cached-root relocation/base42 immediate Hokan pose;
  state4->5 differs from state1 front timer; unknown/disabled gates; late
  bad base/cache/pose rollback and unused Y; exact flags/input/work retention.
- [x] Register pending callback and run targeted test. Expected:FAIL pending.
- [x] Implement source callback and route; targeted tests. Expected:PASS.

## Task2: original retail pose dependencies and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Original graveyard banks: all10 roots, both facings, direct/delayed
  startup, clips401/402/404/405 sampled on original Claire hierarchy, release
  via cached root and original base42. Assert literal source states, count
  native controller/sample events independently; retain all previous probes.
- [x] One fresh read-only source review; material findings one RED/GREEN fix
  pass plus green full suite; Minor findings recorded/deferred.
- [x] tools/build.ps1. Expected:56/56tests and refreshed EXE/ZIP.
  Original205-room runtime scan and fresh packaged dummy-SDL smoke. Expected:
  old probes retained plus standing samples;241ticks/240renders/53moves/
  10focus-blocked ticks/main-menu return/INI persistence.
- [x] Format/diff gates, evidence/rulings ledger, documentation and commit.
  Step1 goal stays active until complete slice acceptance.

Verified:56/56tests;205original rooms;4480standing player states/4440Claire
samples/40cached-root releases; fresh packaged dummy-SDL smoke241ticks/
240renders/53moves/10blocked ticks/menu return/INI creation. Important cache
layout acceptance defect fixed with both invalid layouts observed RED-to-GREEN
and fresh full-suite/package/retail verification. Minor late-attribute rollback
coverage deferred in the status document and ledger. Full Step1 remains active.
