# Native effect resource binding implementation plan

Checkpoint clarification — 2026-10-04: this plan's baseline and proposed gates
describe the earlier binding increment. Current code enables New Game and opcode
173, while Step 1 and its effect consumers remain unfinished. Two resource
validation defects were reproduced in the latest audit. See
[current status](../../step1-slice-status.md) and
[dated audit](../../project-audit-2026-10-04.md). The intended gates and Expected
results below are requirements, not proof of the current working tree.

> **For agentic workers:** Use superpowers:executing-plans inline with
> superpowers:test-driven-development and verification-before-completion.

**Goal:** Bind the actual global/room effect model and texture resources into
the native runtime before original initialization events, retaining palette
memory and source asset indices across room transitions.

**Architecture:** NativeEffectAssets retains the source family index/bank tables,
global models and persistent texture library. SYSTEM.AFS entry1 initializes its
base. The actor decoder exposes its actual texture cursor; prop binding publishes
that cursor. Room loading trims prior room bindings, preserves shared palette
words, resets the automatic cursor32, then loads unique families in source order.
RuntimeView supplies real global resources before its initial load. Existing
ScenarioState ownership carries them across atomic room replacements.

**Tech stack:** C++17, CMake/Ninja, SDL2, GCC, Python, CTest.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
BASE:3037798e1a7f6c20b37810f82760497c6c2a0c47.

## Global constraints

- Complete two-room native gameplay/combat/inventory/audio/save gate stays active.
- Native Windows code with user-owned USA assets; upstream remains pinned.
- All checks headless: SDL dummy video/audio/software, fresh ignored profiles.
- No retail content in Git/packages; no external publication or GUI opening.
- No source/header mutation while any compiler consumes this checkout.
- New Game/173 remain gated until their real full consumers are implemented.

## Source design

system.c loads SYSTEM.AFS entry1 then bhInitEffect. Its ef_info table specifies
four size-prefixed MDL chunks at IDs0/1/7/13. The original cursor reads the next
size after each model and aligns to32 bytes before the texture pack. Retain model
bytes with validated MDL/header/object extents; rendering these original models
uses its separate original draw paths. The20 authored nbAnim counts sum40 and
initialize ef_tn indices; entries20..29 retain their original zero index. All30
base palette-bank values derive from the binding at that index, including255
for the ordinary no-shared-bank representation.

room.c consumes geometry, actual cached enemy/prop/item packs, then the first
occurrence of each family30..99 or400..449, regardless of its work flags. Do not
guess the cursor by subtracting effect counts. Bind family first/count/bank and
retain base textures. source bhInitRoomChangeSystem resets ef_pbnk32 without
clearing palbuf: room replacements must retain prior CPU palette values.
Release old room bindings and charge only retained base plus new emissions.

The public lookup uses original family-first+animation indices, bounded by the
complete texture library. Zero-nbAnim base entries retain source alias behavior;
family counts are metadata, not an invented animation constraint. Unknown room
families and unloaded global resources reject instead of using placeholder art.

## Review focus

1. Exposed actor cursor includes real cache reuse and props/items; inactive
   effect records still consume a unique resource family.
2. Global40-texture offsets and zero-animation aliases match the source table.
3. Room transitions release old bindings but retain palettes/base image identity.
4. Damaged/missing resources reject before entry events and preserve the previous
   runtime, pending door and persistent progress.
5. Native executable boot actually provides SYSTEM entry1 to its runtime.

## Task 1: bind original resources and runtime ownership

Files: create src/effect_assets.hpp,src/effect_assets.cpp,
tests/effect_assets_fixture.hpp,tests/effect_assets_tests.cpp;
modify src/actor.hpp,src/actor.cpp,src/scenario.hpp,src/scenario.cpp,
src/runtime.cpp,src/runtime_view.cpp,tests/room_tests.cpp,CMakeLists.txt;
update docs/native-effect-textures-status.md. Private evidence stays ignored.

Interfaces:
```cpp
struct NativeEffectAssets;
std::shared_ptr<const NativeEffectAssets>
decode_global_effect_assets(const Bytes &system_entry);
void bind_room_effect_assets(const Bytes &room, ScenarioState &state);
const NativeEffectTextureBinding &effect_texture_binding(
    const NativeEffectAssets &, int32_t texture_id, int32_t animation = 0);
Texture resolve_effect_asset(const NativeEffectAssets &, int32_t, int32_t = 0);
// RoomActorModels::texture_cursor and ScenarioState::room_texture_cursor.
// ScenarioState::effect_assets retains immutable current resources.
```

- [ ] Through the existing load_room_runtime API, require a room with a family92
  record and no global assets to reject. Compile room_tests against BASE and run.
  Expected: genuine behavior RED because the existing loader ignores resources.
- [ ] Implement validated global models/source table, shared texture ownership,
  source cursor exposure, staged room family load/lookup and production bootstrap.
  Supply complete synthetic resources to the existing family92 runtime fixture.
  Expected: the missing-global RED passes; family92 event/mode behavior remains.
- [ ] Add global offsets/zero-animation aliases, unique/inactive room families,
  cursor/cache exposure, animation/global-index/bank lookup, retained palette and
  released-image transitions, malformed models/tables/packs/aliases and rollback
  regressions. Use synthetic MDL/40-texture resources, no retail fixtures.
  Run focused asset/room/actor/palette CTest.
  Expected: exact family-first indices/banks and prior-state preservation pass.
- [ ] Run tools/build.ps1, packaged dummy smoke with a fresh UUID profile and
  ZIP/EXE integrity. Recompile genuine global/room runtime probes using actual
  global resources. Expected:40 base textures plus room1/18/12, actor cursors
  5/10/9; family54=4/408=2; corridor120, cemetery15/173at4, revisit139at276.
- [ ] One fresh read-only review of this increment. Address Important/Critical
  findings in one RED/GREEN author pass; record all rulings/minors. Verify staged
  paths and commit locally; retain private evidence and keep fullStep1 active.

Previous goal turn made concrete progress:3037798 resource service and152/152
verification. Current clean state revalidated. Existing continuous user
authorization covers this original resource port/inline execution without a
repeated approval handoff. Control/drawing/scheduler integration remains required
subsequent work; this task does not substitute resource ownership for gameplay.
