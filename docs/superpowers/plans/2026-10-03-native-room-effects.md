# Native room effect binding and shadow control implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; retain authorized inline execution.

**Goal:** Port original room effect allocation, opcode67 visibility and opcode47
shadow creation, including real source shadow control and shared startup slots.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Decode68-byte EF_WRK into native records, bind immutable pool
values and stable authored-slot indices before enemy initialization. Shadow birth
uses the same512-entry pool; its callback consumes current cached enemy matrices
and source ground queries after player control. No device/SDK/ELF execution.
**Tech stack:** C++17, existing native asset/core libraries, CMake/Ninja/CTest.

## Global constraints

- Windows x64; all checks headless, fresh ignored profiles, dummy SDL audio.
- No fabricated room assets, no skipped required opcode or silent substitute.
- This is source allocation/control integration. Original modifier-volume drawing
  and remaining room effect callbacks/assets remain required for complete Step1.
- All selected authored effects are unlinked. Nonzero authored link groups must
  reject explicitly until their source model/link lifetime is integrated.
- Native shadow state2000 ground-cache branch must reject explicitly until its
  original O_WORK pyb cache exists; ordinary ground/joint control is implemented.
- Full two-room gameplay, combat/inventory/audio/saves/reference remain required;
  New Game stays disabled. Keep the existing branch, no push/merge/publication.

## Review focus

1. Original authored effect slots exist before enemy/shadow startup; allocations
   respect flags3, inactive records and immutable copies without remapping IDs.
2. Malformed counts/pointers/values/links and pool exhaustion reject room binding
   without replacing prior live effects or authored-slot indices.
3. Opcode47 uses authored enemy work mapping, four-byte extent, source duplicate
   gate/variant sizing and source full-pool return rather than a flag-only stub.
4. Shadow callback follows retained owner matrices, ground and visibility gates;
   hidden/frozen effects do not advance; invalid batches preserve old pool values.
5. Real entry evidence distinguishes initializer progress from effect drawing,
   enemy gameplay and full-slice acceptance, with unsupported paths still visible.

## Task1: Original records, binding and visibility

Files: room.hpp, new room_effects.cpp, effects.hpp, scenario.hpp/.cpp,
runtime.cpp, CMakeLists.txt and new tests/room_effect_tests.cpp.
Interfaces:
```cpp
std::vector<RoomEffectRecord> decode_room_effects(const Bytes&);
void bind_room_effects(const Bytes&, ScenarioState&);
void set_room_effect_visibility(ScenarioState&, size_t authored, bool visible);
```

- [x] Write literal RED fixtures for68-byte layout, signed/truncated fields,
  malformed count/table/NaN/ID; slots{2,3,3} for reserved/inactive/active births;
  immutable retention, unsupported links and exhausted binding; opcode67
  `{67,2,0,0,0}` hides only mapped slot3, nonzero visibility restores it.
- [x] Build room-effects-tests; expect missing interfaces, then implement bounded
  records and transactional pool/map replacement. Birth flags/id/type/scale/angle
  values are copied, texture=-1, free when!(flags&3), unlinked flags80 cleared.
  Reject any nonzero authored link group before mutation.
- [x] Native runtime binds records after player caches and before initialize_scenario.
  Add a room-loader fixture with an actual67 initialization request; successful
  entry must contain its mapped hidden effect.
- [x] Run native-room-effect-records/binding and room runtime checks; expect GREEN.

## Task2: Source shadow creation and callback

Files: new shadow_effect.cpp, effects.hpp, scenario.hpp/.cpp, runtime.cpp,
zombie.cpp/.hpp, tests/zombie_tests.cpp, new tests/shadow_effect_tests.cpp, CMake.
Interfaces:
```cpp
std::optional<size_t> create_enemy_shadow(ScenarioState&,size_t,const EnemyShadow&);
void request_enemy_shadow(ScenarioState&,size_t authored,uint8_t variant);
void tick_shadow_effects(ScenarioState&);
```

- [x] Write RED creation tests for first-free flags3, id1/flags240081/texture=-1,
  empty/table type1/0, joints/anchor/base sizes, source allocation exhaustion;
  opcode47 variants0..3=>4.5/4/3.5 and4=>6.5/4/4.5, duplicate gate, truncation,
  authored mapping, invalid work and unsupported variant sets800 without birth.
- [x] Write RED timing tests: first callback follows link and sets8000000/mode1;
  second uses source ground height/yaw and clamps joint extents to base..2*base;
  source shadow_flags gates visibility, inactive parent releases, hidden/gated
  effect retains state; invalid matrix/joint or unsupported ground cache rejects
  the complete staged pool. Zombie startup must allocate real shadow slots.
- [x] Implement source creation and retained-joint callback. Default shadow_flags
  follows system.c initializedffffffff. Integrate callback after player control;
  keep ground_wall query side effects. Store zombie shadow_effect slot alongside
  existing startup metadata, preserve it on source reinitialization.
- [x] Run native-shadow-creation/control and zombie-startup tests; expect GREEN.

## Task3: Original initializer evidence, verification and review

- [x] Rebuild the private original entry probe against current production library;
  verify original effects1/46/62 precede startup and47/67 are no longer boundaries.
  Record the next actual unsupported instruction without skipping it. Exercise
  isolated native shadow control with real cached startup poses as diagnostics.
- [x] Full tools/build.ps1 with SDL_AUDIODRIVER=dummy, fresh packaged runtime smoke,
  format/diff/Python/ZIP checks; expect all pass and no retail files in package.
- [x] One fresh read-only review, gpt-6-astra/high, no child/build/runtime mutation.
  Re-grade findings; one RED→GREEN Critical/Important pass plus full green suite,
  defer Minors, ledger every ruling. Commit only local implementation and tests.

## Remaining acceptance

Original effect model/texture loading, active effect callbacks and modifier-volume
drawing still need integration; allocation/control evidence cannot establish
visual shadow parity. Next unsupported initialization/gameplay operations remain
required, followed by actual two-room travel, inventory, combat, decoded complete
audio, versioned save/load and original-game reference acceptance.
