# Native enemy standing-grab implementation plan

> Execute inline with superpowers:executing-plans/test-driven-development.

**Goal:** Complete en01.c::NG02 states0..3 and its original joined PlyDG01
dependency sequence using original USA assets.
**Architecture:** Reuse existing scalar Work staging in zombie_grab.cpp. Add
EXP0 head-damage word78 to extended work. Stage player fields and sound/RNG;
retain prior immutable effect pool/RNG for rollback after effect creation.
Existing complete blood/fragment creators supply authored effect events.
**Tech stack:** Windows x64 C++17/CMake/CTest, original Ninja math and motion.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:**84b16d5 clean; previous goal turn made verified progress with two commits,
58checks/205rooms/240head-effect births/fresh packaged smoke. FullStep1 active.

## Constraints and review focus

- Headless only; SDL dummy/software and fresh ignored data profiles.
- Native source callbacks, private user retail data, no emulated executable.
- Preserve ChgMtn same-motion retention, startup from prior15frame0 or other
  motionframe10, source state0fallthrough only into state1, frame10ct0=80.
- Player damage at frame27 is personality bonus+5 clamped to0. NG02 does not
  decrement ct0; native motion's source last-frame flag causes normal release.
- Release chooses36/236 only for player_id0; all nonzero choose35/235. Player
  handoff mode3=2/control1=5, root flag40000 and no state3fallthrough.
- Source CutHead sets only EXP0_I(78)=-1; do not invent detached-part updates.
  Motion36 emits blood24/cut+sound4877at25. Motion35 zeroes velocity and emits
  fragments33/cut+sound4869at34/blood35. Sound backend playback is separate.
- Terminal exact frame count-1 stops both works and sets modes{4,2,0,0};
  only root clears lowF and8000. No full MainLoop/Nage/PlayerLink claim.
- Validate optional body/cache/personality/motion only when consumed; unknown
  mode3 ignores them. Any late error restores root/body/player, effects and RNG.

## Task1: complete source callback

Files: src/scenario.hpp,src/zombie.hpp,src/zombie_grab.cpp,
tests/zombie_standing_grab_tests.cpp,CMakeLists.txt,src/zombie_effects.cpp comment.
Produces:
`void tick_zombie_standing_grab(ScenarioState&,size_t,ScenarioEntity&,
const std::vector<int32_t>& playing={});`
Consumes complete tick_zombie_neck_blood_effect/head_fragment_effect and
choose_zombie_sound; emits original sound requests, not decoded audio.

- [x] Add pending callback and literal tests; register one CTest.
  ```cpp
  tick_zombie_standing_grab(s,0,p);
  ck(s.enemies[0].motion_number==9 && s.enemies[1].motion_number==209 &&
     s.enemies[0].control0==80 && p.modes==std::array<uint8_t,4>{4,0,1,0},
     "NG02 source startup");
  ```
  Cover both startup motions/facings, literal source offsets and full Y,
  same-motion retained counters; exact frame10/27 gates and source no timer
  decrement; clamped damage; nonzero character choice; release no fallthrough;
  all authored effects/head-cut/sounds/terminal flags; null and invalid body;
  unused context; player alias; rollback after effect birth and fragment RNG.
- [x] Build and run `ctest --test-dir build/windows -R
  '^zombie-standing-grab$' --output-on-failure`.
  Expected:FAIL with native standing enemy grab pending.
- [x] Implement complete states using scalar snapshots, existing matrix/creation
  services and a local rollback guard. Narrow prior effect header's RNG statement
  when documenting source integration (Kami/Water none, fragments four).
  ```cpp
  if(frame==27)player_health=std::max(0,add(player_health,-(bonus+5)));
  // Source does not decrement timer here.
  if(last_frame()){ stop({4,2,0,0}); e.zflags&=~0x800fu; }
  ```
  Expected:new check and prior NG00/NG01/PlyDG00/PlyDG01/head-effect checks PASS.

## Task2: original joined sequence and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Join full NG02 -> PlayerControl/PlyDG01 -> enemy/player motion/cache work
  across10graveyard roots, both facings and both prior startup motions. Use
  native original9/209 last-frame flags, Claire36/236 frame24blood/25cut and
  source exactterminal frame; complete cached-root player release/base42 sample.
  Expected:40joined enemy deaths/player releases and320blood births, one source
  damage event per sequence; all205rooms/prior probes retained. Non-Claire
  motion35 events are literal fixtures; no retail Chris-asset coverage claim.
- [x] One fresh read-only final source review; material issues one RED/GREEN
  correction/full suite; Minor findings recorded/deferred.
- [x] tools/build.ps1:59/59checks, EXE/ZIP refreshed; final205-room scan and
  fresh packaged dummy-SDL smoke241ticks/240renders/53moves/10blocked/menu/
  INI creation. Format/diff gates and local evidence/rulings ledger.
- [x] Commit complete increment; keep Step1 active. PlayerLink/root scheduling,
  full combat/effect playback/audio/inventory/versioned saves remain required.

Final evidence:59/59checks,205rooms;4680joined NG02enemy updates,
4680Claire samples/9280paired enemy samples,40head-cut deaths/40releases/
320blood births. All prior grab/corridor checks pass. Fresh packaged smoke
241ticks/240renders/53moves/10blocked/main-menu return/INI creation.
Review: no material findings. Deferred Minor: terminal-body failure after
fragment creation/RNG lacks a focused regression; late-bank rollback is tested
and the body path shares the same inspected guard. FullStep1 stays active.
