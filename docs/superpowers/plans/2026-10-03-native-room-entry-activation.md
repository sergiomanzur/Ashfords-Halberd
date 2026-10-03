# Native room entry activation implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; preserve authorized inline execution.

**Goal:** Execute the original effect-mode and defeated-enemy entry operations
and bound-work flag controls that stop the selected graveyard initialization.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Original opcode145 writes mode1 through the stable authored
effect map, replacing immutable pool values. Opcode34 reads the source512-bit
defeat range and hides the mapped enemy only when its bit is set. Both retain
the source four-byte cursor extent. Discover subsequent real entry boundaries.
**Tech stack:** C++17, native asset library, CMake/Ninja/CTest, private retail probe.

## Global constraints

- All checks headless; SDL video/audio dummy; isolated ignored profiles.
- Native Windows x64, no retail ELF/SDK execution, no skipped required opcode.
- Keep New Game gated until the complete native playable slice is accepted.
- Preserve the full two-room gameplay/combat/inventory/audio/save/reference scope.
- Work on codex/windows-foundation; no push, merge or publication.

## Review focus

1. Opcode34 masks the source9-bit defeat index, uses MSB-first bits and does
   not resolve an enemy map when the defeat bit is clear.
2. Opcode34 hides only the authored mapped work, never re-enables a previously
   hidden enemy or overwrites unrelated flags/progress; common23/46 change only
   source work bits and46 must consume its full16-bit value.
3. Opcode145 addresses mode1 through the authored effect map, including inactive
   work/aliases, and preserves prior room snapshots and other mode bytes.
4. Truncated instructions or invalid required maps reject before state writes;
   full four-byte instructions resume at their actual next byte.
5. Original initializer evidence distinguishes completed entry from actor ticks,
   effect drawing and full-slice acceptance; newly required services stay visible.

## Task1: Original activation operations

Files: new tests/scenario_entry_activation_tests.cpp, CMakeLists.txt,
src/scenario.cpp, src/effects.hpp, src/room_effects.cpp, tests/room_tests.cpp.
Interfaces: existing run_scenario(Bytes,ScenarioState&) and new
`void set_room_effect_mode(ScenarioState&, size_t authored, uint8_t mode);`.

- [x] Write literal tests using `Bytes{34,0,32,0,55,7,0}` with
  `enemy_slots={2}`, `flags[3][1]=0x80000000`: only enemy2 gets1000000 and
  room_case becomes7. Cover0/31/32/511/512/65535, unset/no-map, retained hidden
  work, source flag preservation and malformed required map/extent.
- [x] Write `Bytes{145,1,255,99,55,9,0}` with `effect_slots={4,31,31}`:
  work31 mode1 becomes255, other bytes remain{2,3,4}; retained snapshot unchanged.
  Cover alias/inactive/full pool, missing pool/map, repeated mode and truncation.
- [x] Compile the test directly against current libashford_assets.a; run each
  mode separately. Expected RED: unsupported34 and145, not fixture errors.
- [x] Implement source operations:
  ```cpp
  const auto count = word(pc + 2);
  if (state.flags[3][(count & 0x1ff) >> 5] &
      (0x80000000u >> (count & 31)))
    indexed(state.enemies, indexed(state.enemy_slots, program[pc + 1]))
        .state_flags |= 0x1000000u;
  // opcode145: extent(4); copy mapped pool, assign works[slot].modes[1].
  ```
  Both handlers advance pc by4. Validate effect mapping before cloning/writing.
- [x] Add an original-shaped runtime initializer145 then67; assert real room
  loader binds the effect before mode/visibility changes. Expected GREEN for
  native-room-entry-defeat/effect-mode and room-entry-records.

## Task2: Real initialization, verification and fresh review

- [x] Recompile the private original entry probe against the production library;
  run unchanged scripts from RDX1/2/3. Expected:145/34 are no longer boundaries.
  Record the actual next unsupported services, with no byte/opcode skipping.
- [x] If additional entry operations are exposed, inspect original source and
  decide a coherent source-backed extension; record plan changes in the ledger
  and apply another literal RED→GREEN cycle before production code.
- [x] Source-backed extension from actual RDX2: common23 changes bound work flags.
  Write a scheduled101/105 fixture for groups1/2/3 and variants0..3/4/255:
  masks clear118/set118/clear18/set18; unmatched variants retain flags.
  `Bytes{101,1,0,0,105,23,0,255,55,6,0}` must modify only mapped work2,
  preserve modes/status/other work, and reach room_case6. Truncated/no-bound/outside
  work rejects before flags change. Expected RED: unsupported common23.
  Implement four-byte source extent and switch, then expected GREEN for
  native-room-entry-work-flags and re-run original probe without skipping139.
- [x] Next actual common46 boundary: source word0 clears20; any nonzero word
  sets20. Write group1/2/3 literal tests with values0/1/256/65535, required work,
  truncation and source four-byte extent. Expected RED: unsupported common46.
  Source common23 unmatched branch never reads work; add no-bound255 fixture,
  expected RED for existing eager lookup, then preserve that source no-change
  branch before required lookup. Expected GREEN: five focused checks and probe.
- [x] Run tools/build.ps1 with dummy audio, a fresh packaged native_runtime_smoke,
  formatter/diff checks and ZIP contents inspection. Expected: full suite green,
  native process exits successfully, package contains no retail assets.
- [x] One fresh read-only gpt-6-astra/high review of the increment, all focus
  areas and source/logs. Re-grade, one Critical/Important RED→GREEN correction
  pass plus full green suite; defer Minors and ledger every declined judgment.
- [x] Update status/ledger, mark checks from actual evidence and commit locally.

Full Step1 remains incomplete until the original connected rooms support their
real gameplay, complete audiovisual behavior, inventory/combat, saves and an
independent original-game comparison.
