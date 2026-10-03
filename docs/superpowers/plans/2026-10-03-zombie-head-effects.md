# Native standing-grab head effects implementation plan

> Execute inline with superpowers:executing-plans/test-driven-development.

**Goal:** Port the complete NeckBloodEffect and ExpHeadEffect creation callbacks
needed by en01.c::NG02, including their SetBlood3/Nikuhen creation services.
**Architecture:** Extend existing immutable512-slot effect work with state flags,
birth counters and a logical model-texture owner. Reuse first-free flags3
allocation and EF scratch retention; stage pool and local RNG before commit.
No render assets or model caches are copied or changed by creation.
**Tech stack:** Windows x64 C++17/CMake/CTest, pinned MIT source and Ninja math.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:**8e2d558, clean authorized checkout.

## Global constraints and review focus

- All checks headless, dummy SDL, fresh ignored diagnostic profiles.
- Native code only; original data private and not included in distributions.
- Full Step1 combat/audio/inventory/versioned-save acceptance remains required.
- Preserve source effect allocation order, partial/full exhaustion and untouched
  EF scratch fields. Birth does not imply effect playback or rendering.
- Blood type!=0 consumes no enemy/cache context. Type0 emits eight linked
  id370 works, joint10, alternating type1/n6 and type3/n3, ct0=0..3.
- Fragment type is ignored by source; four id362/type0 works consume joint10
  point(0,.5,-1), world yaw and four LCG calls before allocation, including full
  pool. Preserve exact source float expression2+(-rand()/-2.1474836E9f).
- Texture remap uses complete model-version groups; model-texture owner follows
  ML_WORK sharing, independently of the linked parent slot. Missing consumed
  owner/cache or invalid matrix fails transactionally; unused context is ignored.

## Task1: complete creation callbacks

Files: src/effects.hpp,src/zombie.hpp,src/zombie_effects.cpp,
new tests/zombie_head_effect_tests.cpp,CMakeLists.txt.
Produces:
`void tick_zombie_neck_blood_effect(ScenarioState&,size_t,int type);`
`void tick_zombie_head_fragment_effect(ScenarioState&,size_t,int type);`
Both leave enemy/model state unchanged and replace effect_pool only on success.

- [x] Write source literal tests and pending callbacks; register one CTest.
  Example source expectation:
  ```cpp
  tick_zombie_neck_blood_effect(s,0,0);
  ck(s.effect_pool->works[0].id==370 &&
     s.effect_pool->works[0].linked_joint==10 &&
     s.effect_pool->works[0].control1==6,"source neck blood birth");
  ```
  Verify eight directions/counts, scratch retention/narrowing, first-free order,
  no blood RNG, all256texture versions, null unused context, shared texture
  owner/current parent, full/partial pool, invalid owner/cache and matrix rollback.
  Fragment fixtures use literal translated/rotated joint10 and seed1's first
  four known LCG results, source float scales and full-pool RNG consumption.
- [x] Build target and run `ctest --test-dir build/windows -R
  '^zombie-head-effect-creation$' --output-on-failure`.
  Expected:FAIL with native neck blood creation pending.
- [x] Implement source callbacks and private creation services using existing
  allocator; extend table_effect to return its optional allocated slot.
  ```cpp
  seed=seed*1103515245u+12345u;
  const auto roll=seed&0x7fffffffu;
  ef.scale={2.f+(float(-int32_t(roll))/-2.1474836E9f),0,0};
  ef.scale.y=ef.scale.x;
  ```
  Validate only consumed texture/cache context after successful allocation.
  Expected:new test and existing zombie-effect creation/routing PASS.

## Task2: original caches and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Add per-root original graveyard birth probes: eight blood/four fragment
  works, correct parent versus texture owner, fragment position computed from
  original joint10, four random calls, no raw/cache mutation; all10roots.
  Retain205-room, grab and corridor checks.
- [x] One fresh read-only final source review; material findings one RED/GREEN
  correction and full suite; Minor findings recorded/deferred.
- [x] Full tools/build.ps1:58/58checks, EXE/ZIP refreshed. Final205-room scan
  and fresh packaged dummy-SDL smoke241ticks/240renders/53moves/10blocked/
  main-menu return/INI creation. Format/diff gates and local evidence ledger.
- [x] Commit complete increment. NG02 timing/head-cut word/root dispatch,
  effect playback, full combat/audio/inventory/save work remains required;
  keep Step1 goal active and New Game disabled.

Verified:58/58headless checks;205rooms;ten paired root/body probes and240head
effect births with retained original caches; all existing grab/corridor probes.
Fresh packaged smoke241ticks/240renders/53moves/10blocked/menu/INI creation.
No material review findings. Deferred Minor: existing file header says births
consume no RNG; correct for Kami/Water, but fragment creation consumes four
rolls as specified and tested here. No playback or NG02 integration claim.
