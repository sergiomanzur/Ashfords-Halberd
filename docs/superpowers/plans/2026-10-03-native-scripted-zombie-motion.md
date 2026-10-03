# Native scripted zombie motion implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; preserve authorized inline execution.

**Goal:** Execute the selected original common24 motion selection, its linked
body/model/effect calculations, and subsequent source event-mode animation.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Move source motion selection to a focused native service;
stage entry changes, retain pose-owner aliases and original matrix update order.
Source bhSetMotion arguments remain separate from stored step/mode. Event-mode
callbacks consume their original subpl table before the zombie MainLoop SetMtn.
**Tech stack:** C++17, native matrices/motion/actors, CMake/Ninja/CTest.

## Global constraints

- Windows x64; all checks headless, dummy audio/video, fresh ignored profiles.
- No SDK/ELF execution, opcode skips, fabricated retail assets or silent stubs.
- Keep New Game gated and the full connected-room Step1 acceptance intact.
- Existing codex branch and authorized inline workflow; no push/merge/publication.
- Selected real request: RDX2 pc314, enemy slot3/id1, speed1, selector1,
  motion4, event_mode1, immediate0, blend6. Original body receives motion5.
- Other selector0/2/3 paths remain explicit required boundaries when encountered;
  implement every immediate0..7 action of the selected selector1, not just fields.

## Review focus

1. Source selector1 preserves untouched counters/modes, initializes its actual
   companion through the source body index and retains shared pose ownership.
2. Immediate bhSetMotion uses add0/mode0, samples once without frame advancement,
   and does not accidentally call zombie SetMtn effects or sample the companion.
3. CalcModel/CalcLinkModel respect linked order and mutable shared caches,
   retained per-work matrices, hidden linked actors and linked effect matrices.
4. Event-mode controller runs its source callback before zombie SetMtn, preserving
   single/double sampling, blend/frame state, movement signs and unsupported gates.
5. Malformed banks/body/link/cache/instructions retain staged entry state; original
   probe evidence still distinguishes script progress from full playable acceptance.

## Task1: Model and selected motion integration

Files: new src/scenario_motion.hpp/.cpp and tests/scenario_model_tests.cpp,
tests/zombie_script_motion_tests.cpp; src/scenario.cpp, effects.hpp,
zombie.cpp, zombie_motion_internal.hpp, CMakeLists.txt.
Interfaces:
```cpp
struct ScenarioMotionSelection {
  uint8_t speed, selector, motion, event_mode, immediate, blend;
};
void select_scenario_motion(ScenarioState&, uint8_t group, size_t work,
                            const ScenarioMotionSelection&);
void calculate_scenario_models(ScenarioState&, uint8_t group, size_t work,
                               bool self, bool links);
```

- [x] Write original-shaped scheduled101/105/24 RED tests using root/body slot2/3,
  room motions4/5 and `Bytes{101,1,0,0,105,24,1,1,4,1,0,6,55,7,0}`.
  Expect root mode5/motion4/body mode6/motion5, root frame0, companion frame0,
  both room bank, independent base bank preserved, no implicit body pose sample.
  Immediate1 changes fields only;0/2/5/6 sample root with add0/mode0;
  0/3/5/7 calculate self;0/4/6/7 calculate links. Blend6 clears body count
  but preserves root count until sampling; >8 sets both; root frame-direction
  stays while body resets. Invalid body/bank/speed/selector/link/extent rejects.
- [x] Watch script test RED against current production library: unsupported
  motion selection mode/companion. Model test compile/link RED: missing service
  or effect matrix. Do not add production code before these failures.
- [x] Implement generic source CalcModel and CalcLinkModel using stable owners,
  active linked enemy/object/item order and active linked effect matrix updates.
  Unit fixture root position100, root poseX2, linked body at joint1 and one
  shared-cache auxiliary at joint0 must leave cacheX106; root baseX100, body
  baseX102, auxiliary baseX104. Effect joint1 getsX106/Y202. Prior pool immutable.
- [x] Separate internal motion apply arguments from stored values:
  `apply_zombie_motion_channels(state,work,add,mode)` for immediate0/2/5/6.
  Existing regular callers retain source stored motion_step/motion_mode.
- [x] Replace common24's inline restricted branch with selected service call.
  Publish staged vectors/cache/effect/scratch only, preserving live task context.
  Expected GREEN: native-scripted-motion-selection and native-scripted-models.

## Task2: Event controller and real original probe

Files: src/scenario_motion.cpp/.hpp, zombie_controller.cpp, scenario.cpp;
tests/zombie_script_motion_tests.cpp.
Interface: `void tick_scripted_enemy(ScenarioState&,size_t work);`.

- [x] Write RED event tests for source ct2 table0..14: static/init controls0/2/6/10,
  sampling controls4/5/8/11..14, turn/movement1/3/7/9, mode3 hold gates and invalid
  index rejection. Literal table expectations:6 resets blend0 without callback
  sampling;8 samples one frame;5 sets ping-pong mode100/count16/step65536.
  Source controller5 callback followed by zombie SetMtn must retain its original
  extra sample. Existing body6 bypass must remain unchanged.
- [x] Implement source subpl callbacks and wrapped target/turn/movement operations
  with small scalar staging and shared validated pose sampling. Integrate zombie
  mode5 before MainLoop's existing grab/motion/neck phases; support generic native
  subpl event modes5/6 with calculation and ordinary damage cleanup afterward.
- [x] Run focused event/selection/model checks and existing zombie/actor tests;
  Expected GREEN. Recompile unchanged original entry probe against production;
  expect common24 advances, record actual next entry/gameplay service, no skips.

## Task3: Full verification and one fresh review

- [x] tools/build.ps1, fresh packaged dummy native_runtime_smoke, format/diff/ZIP
  whitelist. Expected full suite and native packaged process pass.
- [x] Update status and ledger from original evidence. One fresh read-only
  gpt-6-astra/high review of all focus areas/source/logs, no child/runtime/tools
  mutation. Re-grade, one Critical/Important RED→GREEN correction pass plus full
  green suite; defer Minors, record every declined judgment and local commit.

Remaining full Step1 requirements include complete room entry/AI/gameplay,
inventory/combat, original effect drawing/audio, cross-room saves and independent
reference acceptance. Opcode139 computer event0 remains required separately.
