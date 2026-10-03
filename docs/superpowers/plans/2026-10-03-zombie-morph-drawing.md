# Native zombie morph drawing implementation plan

> Execute inline with superpowers:executing-plans; request one final source review.

**Goal:** Draw original zombie mouth morphs after CalcEnemy, preserving its
cached matrices and the immutable retail model bank.
**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
**Architecture:** Prepare ordinary skin coordinates once using translation-only
bind offsets. Store immutable prepared source/target models separately from raw
retail assets. Each draw creates a private morphed model with current source
object positions, interpolates eligible vertices/normals, then uses existing
cached matrix skin rendering. Geometry changes never refresh collision caches.
**Technology:** C++17, existing actor renderer, Ninja float math, CMake/CTest.

Source anchors: njplus.c npSkinConvPreparation/Sub, npCalcMorphing/npTransform,
npPush/PopMdlstr2; eneset.c bhDrawEnemy; en01.c Init obj_a/b and SetMtn mouth.

## Task1: native preparation and morphology

Files: actor.hpp, actor_render.cpp, new actor_morph.cpp, new
tests/actor_morph_tests.cpp, CMakeLists.txt.

- [x] Add pending interfaces and literal tests, observe RED. Cover skin bind
  conversion before pose changes, rigid/skin output equivalence, evalflags8 and
  missing models, current source object translation versus target bind position,
  source multiplication order, extrapolation (no clamp), vertex count mismatch
  preserving the position update, unchanged XYZ preserving source normals,
  changed XYZ normal interpolation, raw W lanes and terminal255, selected range,
  malformed consumed extents and immutable inputs.
- [x] Implement `prepare_actor_skin` and `morph_actor_model`; isolate geometry
  scratch from retained models. Preserve model descriptors and append separate
  morphed vertex streams as source redirects vlist. Validate consumed data;
  ignore absent/skipped target geometry. No face layout conversion or gameplay
  effect substitution. Run targeted actor/morph checks GREEN.

## Task2: cached zombie drawing integration

Files: scenario.hpp, zombie.hpp, new zombie_draw.cpp, zombie.cpp,
tools/room_inspect.cpp, src/runtime.cpp, tests/actor_morph_tests.cpp,
docs/scenario-status.md.

- [x] Retain immutable prepared pair at eligible root startup when bank0/1
  exists; raw owner.model remains unchanged. `draw_zombie_model` resolves owner
  pose/cache, applies own source mdflg2 shape gate and bypass under st_flg20,
  draws selected range and returns a scene. It does not advance motion/cache.
  Keep unsupported mdflg4 recalc explicit. Add RED/GREEN draw regressions.
- [x] Compose initialized zombie works through the cached source drawer rather
  than rejecting supported morph/linked/retained-matrix modes. Preserve source
  linked parent stflg1000000 visibility gate. Observe runtime composition RED
  before integration, then GREEN without changing unsupported generic modes.
- [x] Use source drawer at attached startup and in retail paired SetMtn/CalcEnemy
  probe; compare active mouth with same-cache zero shape. Verify actual changed
  corners, raw assets/pose/cache unchanged, existing4800zero drift retained.
  Full root scheduling, clipping/lighting, effects/combat/audio/inventory and
  cross-room save/load remain separate; do not enable New Game.
- [x] Final read-only source review, fix substantive finding once with RED/GREEN.
  Full tools/build.ps1, retail205-room scan, fresh packaged dummy-SDL smoke,
  diff/format checks. Record evidence and commit progress; full Step1 stays active.
