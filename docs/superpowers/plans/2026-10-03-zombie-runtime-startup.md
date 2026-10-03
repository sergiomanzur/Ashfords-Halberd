# Original zombie runtime startup integration

Execute inline with executing-plans/TDD; base17589d3. Specification:
docs/superpowers/specs/2026-10-02-windows-port-design.md. Selected original
rooms and full acceptance: docs/step1-room-selection.md.

Goal: replace the actual ID1/mode0 startup scheduler block with the original
root/body frame sequence, supplying retained player work from RoomRuntime.
This is a required integration phase; ordinary movement/combat, inventory,
audio, saves and final two-room/reference acceptance remain required.
New Game stays disabled. No unsupported state is treated as an empty callback.

## Task1: original root/body startup frame

Files: src/zombie_controller.cpp,src/zombie.hpp,src/scenario.cpp/.hpp,
tests/zombie_controller_tests.cpp,CMakeLists.txt.

- [x] Failing scheduler fixture for two original-model-layout roots, linked
  bodies/auxiliary initialization in the same pool pass and supplied player.
  Expected: current ID1 unsupported-controller failure.
- [x] Wire the existing Init, PlayerControl, SetMtn, SearchNeck, CollCheck,
  PlayerLink and CalcEnemy services in source order. Clear player primary4 in
  Init; retain source unsigned byte47 and positive word48/52 countdowns.
  Existing startup transaction replaces the state: reacquire all references.
  DmgCheck's true hit branch and unimplemented ordinary modes fail explicitly.
  No per-frame actor/model copies. No player pointer retained in ScenarioState.
- [x] Verify disabled/frozen/hidden-parent gates, same-pass allocation,
  doubled Init/MainLoop samples, cleanup ordering, missing player and later
  unsupported-state failures. Expected: new focused scheduler test passes.

## Task2: healthy player work and actual room entry

Files: src/runtime.cpp/.hpp, native original-room diagnostic and documentation.

- [x] Bind actual player model, base motion bank, pose/cache, floor, geometry,
  health, flags and modes before original enemy initialization. Pass retained
  player work to normal frame scheduler. Preserve source normal-anchor update
  threshold and its distinction from front/movement anchors. Keep collision
  displacement visible to subsequent player movement and rendering.
- [x] Run actual load_room_runtime for both authored graveyard cases with
  private original assets and report the next real integration boundary.
  Expected: ID1/mode0 block removed; any further unsupported instruction or
  mode remains an explicit failure, never a bypass or gameplay-completion claim.
- [x] Full headless build/package, fresh dummy-SDL packaged smoke, format/diff
  checks and one fresh read-only source review. Material findings one RED/GREEN
  fix pass; minors deferred. Commit verified integration and remaining boundary.

## Review focus

Inspect stale references after startup state replacement, root/body allocation
and same-pass ordering, player lifetime/cache fidelity, field synchronization
and collision displacement, unsigned/signed timer behavior, damage cleanup
after success, and explicit handling of unfinished modes. Verify no skipped
enemies, script substitutions or premature New Game enablement.

## Evidence before final gate

Source scheduler RED: Unsupported scenario actor controller room0 actor1 mode0.
Player binding RED: Native runtime player work binding pending. EventSkipSet
RED: Unsupported scenario opcode106. Script/work synchronization RED: original
script player flags lost. Final focused3/3 GREEN: startup, enemy frame dispatch,
scenario boundaries. Source Init/body ordering, two samples, unsigned47 and
positive48/52 timers, cleanup/gates and retained player bank/cache are verified.

Original native entry diagnostic exited0:10roots/10bodies/52auxiliaries across
both graveyard variants,120corridor work/cache/capsule ticks and60normal-anchor
updates. Both actual load_room_runtime calls explicitly fail at opcode148 at10
(original bhBGSeOn2), after complete scheduled startup. Original205room runtime
scan exits0. The stronger --transition-check correctly fails at the same148
audio boundary: no successful transition or playable encounter is claimed.

Final69/69 headless checks and packaging exited0. The room-entry fixture was
updated from a one-joint prop-shaped player to12joints/12motion tracks so source
player startup's required joints7/11 exist; its prior assertions still pass.
Fresh packaged smoke exited0:241ticks/240renders/53moves/10focus-blocked ticks,
main-menu return and INI creation. Repeated original entry diagnostic and
205room scan both exited0; their reported148 entry boundary remains explicit.
Format/diff checks pass. Fresh source review found no Critical/Important
defects. Deferred Minor: linked-fire boundary regression fixture. This plan's
startup integration is complete; full Step1 and New Game acceptance are not.
