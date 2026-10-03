# Native zombie effect creation implementation plan

> Execute inline with superpowers:executing-plans; request one final source review.

**Goal:** Run original KamiEffect/WaterEffect event routing and required blood,
particle, ripple and splash creation in native SetMtn using source pool semantics.
**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Architecture:** Retain an immutable shared native512-slot effect-pool value.
Active events stage only pool scalars and effect work, then replace its pointer.
Motion rollback retains the prior pool pointer and consumed enemy damage fields.
Skin/model/scene assets remain separate. Ground and water queries use original
static-then-dynamic records, float arithmetic and stable merged record indices.
**Technology:** C++17/CMake/CTest, existing Ninja float matrices, pinned MIT source.

Source anchors: en01.c KamiEffect/WaterEffect and11/18-row tables; zonzon.c
SetBlood2 types1/4; effsub3.c AllocOwork/SetEffParticle and PtclDat01;
effect.c SetEffect/SetEffectTb; hitchk.c GroundPosition/InnerTriangle3/CheckWater;
pwksub.c WaterSplash2; room.c InitReadRDT clears effects.

## Rulings and limits

- Creation does not consume RNG: particle initialization occurs later through
  effsub3 function callbacks. Preserve this timing; do not invent birth RNG.
- Preserve global EF template fields that source calls do not overwrite,
  first-free flags3 allocation, particle groups reserving three slots each,
  and partial-group failure freeing only that group's successful reservations.
- Store logical effect texture slot4 for source global texture lookup; actual
  texture binding, effect updates and draw output remain pending. Birth work is
  native state, not a claim of visible effect playback.
- Ripple's source POINT pointer aliases cached matrix storage; only consumed
  XYZ are meaningful for effect108. Unused trailing offset words stay zero.
- Water yaw transform's uninitialized source ps.y is unused by its consumed XZ
  and overwritten outputY; initialize it to0 in native code.
- Retain Kami negative frame matches (its four entries are not sentinel-gated).
  Water frame-1 is a terminating entry. Duplicate-frame water entries all emit.
- EF_WORK stores signed16-bit ax/ay and floor, and uint16 model version.
  Effect work widens angles and narrows floor/version using source low bits.
  Room entry clears effect slots while retaining the global EF scratch value;
  the native ground-wall index expires with its room.
- No full root MainLoop/controller/combat, inventory/audio/save completion;
  New Game remains disabled until the complete Step1 encounter is implemented.

## Task1: ground/water queries and native effect state

Files: collision.hpp, new collision_ground.cpp, new effects.hpp,
scenario.hpp, new tests/zombie_effect_tests.cpp, CMakeLists.txt.

- [x] Add pending source query/callback interfaces and literal RED tests.
  Ground tests cover types0/2/4/6/7, triangle signed dimensions/boundaries,
  raw slope height, negative distance selection, zero-height fallback, type7
  epsilon, strict nearest/tie order and original -1000/10000 sentinels.
  Water tests cover first matching static/dynamic water, inclusive XYZ,
  flags/type/prm0 filters and ignored context.
- [x] Implement source float queries and stable merged indices. Define native
  pool/work/template fields with default source zero storage and logical
  particle bindings; no model/scene copies. Run source query GREEN.

## Task2: complete event routing and creation

Files: new zombie_effects.cpp, new zombie_effect_tables.hpp, zombie.hpp,
zombie_set_motion.cpp, runtime.cpp, new source-table verifier, tests above.

- [x] Observe RED for pending routers/table verification, then port all public
  table constants. Test Kami first-match/j0 extra particles, damage joint/yaw
  direction, normalization/color levels, linked work and flags2 reservations;
  pool full and partial group failure, no RNG, template retention, unmatched
  ignored inputs, Water duplicate frames/entry termination, yaw joint offsets,
  ring-before-splash order, scales and ground-to-water gating, late-error
  atomicity. Required particle birth set1 covers both original PD_WORK entries.
- [x] Integrate native effect handlers with existing synchronous SetMtn hooks.
  Retain prior pool pointer and damage fields for rollback after later failure.
  Add dynamic floors and clear transient pool/floors at room load. Test source
  pre-sampling frame routing and rollback without copying runtime/assets.
- [x] Replace retail diagnostic dry stand-ins with actual source routers;
  exercise original bite/step effect births against actual cached models.
  Preserve all1600SetMtn/CalcEnemy,30mouthdraw/4800zero-drift checks and205rooms.
- [x] Final read-only source review and substantive fix RED/GREEN if needed.
  Full tools/build.ps1, fresh packaged dummy-SDL smoke, final retail scan and
  diff/format checks. Record evidence and commit progress; full Step1 stays active.

## Verification evidence

Final tools/build.ps1:45/45 headless checks, executable and ZIP refreshed.
Retail final205-room scan:1600paired SetMtn/CalcEnemy,3200native effect routers,
10blood/particle and10duplicate step birth probes,4800exact zero-drift paths,
30same-cache mouth draws changed, raw models and caches retained.
Fresh packaged dummy-SDL profile:241ticks/240renders/53movement ticks,
10focus-blocked ticks, main-menu return and INI persistence passed.
Final source review: scratch retention and signed16 splash yaw corrected with
RED-to-GREEN regressions. Rounded Triangle3 height boundary also corrected with
RED-to-GREEN regression. Final suite45/45; format and diff checks pass.
Full Step1 remains active: effect playback and complete encounter still pending.
