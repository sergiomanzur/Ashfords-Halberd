# Native original scenario collision activation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; preserve authorized inline execution.

**Goal:** execute original wall/misc/floor activation commands and make real
runtime collision/interaction consumers observe their changes.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**BASE:**1659422. Continue full Windows Step1; do not reduce acceptance.
**Architecture:** source event10/11/12 four-byte handlers update their actual
static authored ATR tables, retaining other bits. Keep native interaction and
collision views coherent; movement must consume current scenario collision,
not the unchanged decoded runtime snapshot. Stage validation before mutation.
**Tech stack:** C++17, native collision/scenario/runtime, CMake/Ninja/CTest.

## Global constraints

- All verification headless, dummy output and fresh ignored profiles.
- Preserve existing checkout/inline workflow; no push/merge, no SDK/ELF runtime.
- No opcode skips, forced script branches, fabricated assets or field-only stubs.
- Real RDX2 pc364 instruction{11,2,1,0}: disable type4 interaction2 in pool5.
  Original wall9/36/21 disable commands follow. Source event.c functions
  bhWalAtariSet1685, bhEtcAtariSet1717, bhFlrAtariSet1749.
- Keep AI/computer139, combat/inventory, audio/effect drawing, saves/reference
  acceptance required. New Game stays gated.

## Review focus

1. Exact four-byte cursor, byte2zero enables/nonzero disables, untouched flag
   bits/fields, original static table indices and invalid extent/index rollback.
2. Collision/interaction views stay coherent, including existing miscellaneous
   mutations14/35/37; validation precedes flags/items/progress publication.
3. Player movement and enemy query consumers see current scenario collision;
   decoded snapshot cannot silently retain walls disabled by actual scripts.
4. Real probe advances untouched original initialization; do not infer full
   cemetery gameplay from entry or skip the next missing source controller.
5. Existing room transitions/persistent fields, malformed binding recovery,
   headless package evidence and complete-Step1 boundaries remain accurate.

## Task1: Source command and consumer tests before implementation

Files: new tests/scenario_collision_activation_tests.cpp; later source helper
src/scenario_collision.hpp/.cpp, scenario.cpp, runtime.cpp and CMakeLists.txt.
Interface:
```cpp
void set_scenario_collision_visibility(ScenarioState&,uint8_t table,
                                       size_t index,bool enabled);
```
Tables0wall/1misc/2floor. Actual complete-room binding must validate needed
views before publishing a flag; source unbound test fixtures may keep only
interaction storage for old miscellaneous handlers, without inventing assets.

- [x] Write RED literal opcodes10/11/12 for zero/nonzero variants, flagsa5
  retention and room-case cursor checks; misc index2 must disable actual
  examination request. Add malformed extent/index/binding unchanged-state tests.
  Expected RED: unsupported opcode11 in old production.
- [x] Write RED consumer checks for native wall/step/floor queries and runtime
  locomotion using a stale decoded snapshot versus current scenario collision.
  Expected RED: current runtime still passes runtime.collision to movement.
- [x] Implement three source commands and coherent misc mutation helpers for
  existing14/35/37. Validate required copies before item/progress/flag mutation.
  Bind actual runtime collision consumers to current scenario data, preserving
  transition safety and input timing. Expected GREEN: new focused cases plus
  existing scenario-entry, collision, interaction and runtime checks.

## Task2: Original entry, full verification and one fresh review

- [x] Recompile unchanged real production probe, execute headless selected
  original rooms; record next original service and actual gameplay/ticks.
  Expected: opcode11 and following wall toggles advance without skips.
- [x] tools/build.ps1, fresh packaged dummy smoke, format/diff/ZIP whitelist.
  Expected full suite/build/packaged process pass.
- [x] Update status/ledger; one fresh read-only gpt-6-astra/high review against
  all focus areas/source/logs, no writes/runtime/children. Re-grade; fix only
  Critical/Important once via RED-to-GREEN and full suite, defer Minors; ledger
  every declined judgment, local commit, leave full Step1 active.
