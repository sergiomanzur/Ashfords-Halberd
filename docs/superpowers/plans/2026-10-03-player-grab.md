# Native player held-grab implementation plan

> Execute inline with executing-plans/TDD; one final fresh read-only review.

**Goal:** Port complete en01.c::PlyDG00 and EnemyPushChk, the player-side
held/lunge grab controller needed by the native combat slice.
**Architecture:** Stage player/root motion and scalars, touched nearby enemies,
linked-body mode byte, input mask, voice/sound requests and RNG. Share immutable
motion banks; no actor assets or cached poses are copied per action tick.
**Tech Stack:** C++17/CMake/CTest; pinned MIT en01.c/zonzon.c source.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.

## Global constraints and source decisions

- All verification headless; fresh diagnostic profiles; retail assets ignored.
- Existing codex/windows-foundation checkout and authorized inline work continue.
- PlyDG00 handles player mode2=0, all mode3 states0..8, with source fallthrough.
  Standing-grab PlyDG01, player matrix linking and enemy NG00 damage remain
  separate required dependencies; dispatch rejects enabled unsupported mode2.
- Source ayp is signed32; waxp is signed16. Add distinct fields rather than
  conflating authored angular_speed with controller yaw correction.
- Player EXP1 status word and base bank are explicit optional retained work.
  Missing consumed work fails before committing state. Unknown mode3 consumes
  no context. Input pad_on low-nibble clears only on release/death.
- EnemyPushChk scans original128 slots, including id26 with extended work.
  Mode1 consumes RNG before distance and returns from the entire scan when the
  roll is nonzero. Source writes mode0=3 to the attacker's exp1 linked body,
  not the pushed enemy's body; preserve that source side effect.
- Voice requests retain CallPlayerVoice tokens1025/1026 and suppression on
  attacker primary10000; decoding/playback remains a required backend.
- New Game stays disabled; this dependency does not complete Step1.

## Review focus

- Exact signed16 wrap and signed32 truncation during five-tick alignment.
- Missing late release bank or invalid nearby geometry rolls back all changes.
- Source random early return and full three-dimensional strict distance10.
- Each character/facing chooses original held, lunge, release and death clips.
- Release preserves unrelated player flags/input and retains source motion mode.

## Task1: native controller and dependencies

Files: src/player_grab.cpp, src/zombie.hpp, src/scenario.hpp,
tests/player_grab_tests.cpp, CMakeLists.txt.

Interfaces:
`tick_zombie_player_grab(ScenarioState&, size_t, ScenarioEntity&, const
std::vector<int32_t>& playing={})`; requires gated source player mode2=0.
`push_zombie_grab_neighbors(ScenarioState&,size_t,const ScenarioEntity&,int)`.

- [x] Write regression fixtures with distinct enemy/base motion banks and
  literal character/facing clips, signed alignment, release/death/input flags,
  voice suppression, push metadata/RNG and late rollback checks.
  Register player-held-grab and zombie-grab-push with pending functions.
- [x] Configure/build targets and run `ctest --test-dir build/windows -R
  'player-held-grab|zombie-grab-push' --output-on-failure`.
  Expected: both FAIL because the native dependencies are pending.
- [x] Implement source all states and neighbor push with scalar staging.
  Expected: same two tests PASS, including rollback and flag gates.

## Task2: original-bank probes and final verification

Files: tools/room_inspect.cpp, docs/scenario-status.md, this plan.

- [x] Extend each retail lunge capture with PlyDG00 alignment, original
  player clips408/409, escape410/411 and death412/413; apply original motion
  channels to Claire's actual hierarchy and validate finite caches.
  Expected: all10 graveyard roots and both facing branches pass.
- [x] One final fresh source review; fix material findings RED-to-GREEN.
- [x] Run tools/build.ps1 (expected all CTest pass and refreshed package),
  full205-room runtime scan, fresh packaged native_runtime_smoke.py (expected
  241ticks/240renders/53moves/10focus-blocked/menu/INI), format and diff checks.
- [x] Record actual results and commit this complete increment.

Verified53/53headless tests,205retail rooms,2340player-grab state calls,
2260original Claire motion/cache samples,20escapes and20fatal releases. Fresh
packaged dummy-SDL verification:241ticks/240renders/53moves/10focus-blocked,
main-menu return and INI persistence. Executable/ZIP refreshed.
Fresh source review found no Critical/Important production defects. Deferred
Minors: combined push plus failed-release-bank rollback fixture and exact
negative-angle truncation fixture; separate rollback and positive/exact180
alignment checks pass, and current source implementation is correct.

Rulings/costs retained for continuing Step1:

- Complete only mode2=0 here and explicitly reject standing mode2=1; standing
  captures require complete PlyDG01 before integration.
- Queue original voice tokens; audible decoding/selection/playback still need
  backend validation.
- Reuse authorized named checkout; edits share this workspace until milestone
  integration, preserving established build paths and previous work.
- Enemy NG00 damage and PlayerLink positioning remain separate required work;
  probes select release/death manually, so full gameplay coupling is unverified.
- Retail clips use Claire, the selected initial-slice player; other characters
  require original-bank compatibility checks before enabling their content.
- Keep New Game disabled; the full milestone still requires combat, audio,
  inventory, cross-room save/load and independent acceptance verification.
- Parent executes verification independently of the read-only reviewer; all
  gates must be repeated after subsequent production fixes.
