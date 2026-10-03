# Native enemy targeting implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, with test-first changes and one source review.

**Goal:** Port original enemy view-cone, neck turning and floor-trigger queries needed by the root zombie AI.

**Architecture:** A focused `enemy_targeting` module owns pure queries and bounded neck updates. View checks preserve the two Ninja matrix rotations and three-dimensional normalization. Model-work wrappers read cached matrices, resolve stable pose ownership and commit only the final neck yaw. JointPose retains signed32 object angles; sampled motion channels remain unsigned16 words. Floor queries retain static-before-dynamic order and original inclusive XZ bounds.

**Tech Stack:** C++17, CMake/Ninja, headless synthetic and private retail checks.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Windows x64 native code from pinned MIT source, immutable private retail assets.
- All checks headless; use fresh ignored diagnostic profiles.
- Targeting dependencies do not complete root AI, combat or the playable slice; New Game remains disabled.

## Review focus

- Preserve source strict view comparison, height contribution and zero-cone fallback32768.
- Rotate twice using the Ninja quarter table; adding angles and using a single cosine changes boundary behavior.
- Keep source atan truncation, signed returned yaw and special valid yaw-1 remap to-2.
- Linked neck rotation uses parent yaw and shared model work without recalculating matrices or touching other pose fields.
- Floor triggers ignore Y and return the first qualifying static record before dynamic records; no floor-trigger policy is inferred from unrelated attributes.

## Task 1: Pure targeting and trigger queries

**Files:** create `src/enemy_targeting.hpp/.cpp`, `tests/enemy_targeting_tests.cpp`; modify `CMakeLists.txt`.

**Interfaces:** `int32_t search_enemy_player(Vec3 probe,int32_t yaw,Vec3 player,int32_t cone)`; `int32_t enemy_direction(Vec3 origin,Vec3 target)`; `int32_t turn_enemy_neck(Vec3 origin,Vec3 target,int32_t neck_yaw,int32_t limit,int32_t actor_yaw)`; `const CollisionRecord* enemy_floor_trigger(const RoomCollision&,const std::vector<CollisionRecord>& dynamic,Vec3 position,int32_t floor,uint8_t id,uint8_t action)`.

- [x] Write test-first exact cardinal/diagonal yaw, signed seam, valid-1 remap, strict cone boundaries, vertical separation, coincident vectors and zero/word-wrapped cone cases. Verify expected unsupported failure with temporary explicit stubs before implementation.
- [x] Pin neck snap range,910-word steps, wrap, both9102 limits, actor yaw subtraction and zero-target-angle branch, including limits narrower than a step.
- [x] Pin floor-trigger active/type/id/action/floor filters, inclusive XZ boundaries, ignored Y, static-before-dynamic order, zero/negative extents, nonfinite/overflow rejection and no-match.
- [x] Implement source math and pure queries, run targeted tests and format checks.

## Task 2: Cached model-work neck wrapper and retail evidence

**Files:** modify `src/enemy_targeting.hpp/.cpp`, `tests/enemy_targeting_tests.cpp`, `tools/room_inspect.cpp`, `docs/scenario-status.md`.

**Interface:** `void rotate_zombie_neck(ScenarioState&,size_t enemy,size_t neck,const ActorModelWork& player,size_t player_neck)` adapts `bhEne01_RotNeck`.

- [x] Test root and linked-body yaw/pose ownership, untouched work fields, prior cached joint origins, missing parent/model work/joint/pose rejection with no partial yaw writes; observe explicit unsupported failure before implementation.
- [x] Exercise actual initialized graveyard body neck matrices against Claire joint5; inspect authored floor queries and actual personality view-cone angles, preserving raw assets and explicitly separating diagnostic probes from the pending full root frame controller.
- [x] Run full build/CTest/package, private retail scan, fresh packaged headless smoke, source review and diff/format checks; record evidence and commit.

## Source evidence

`pwksub.c::bhSearchPlayer`; `en01b.c::bhSearchPlayer2`, `bhEne01_SideRotNeck`, `bhEne01_RotNeck`; `zonzon.c::NitenDir_ck`, `bhEne_PosCheck`, `bhEne_EnemyAtariCheck`; `ps2_NaMatrix.c::njRotateY`, `njCalcVector`, `njUnitVector`, `njInnerProduct`.

## Source review fix

The reviewer found the initial signed neck angle was narrowed by the pre-existing JointPose representation. A wrapper regression with raw-2000 and forward target failed (64446 versus source62626). JointPose and bind decoding now retain signed32 object rotations, while motion/scratch interpolation explicitly retains unsigned16 word behavior. Pose-array assertions were updated for the wider representation; rendering still masks angles to source words. The regression then passed and pins next-update canonical output63536. Full-suite and final retail/package verification follow this fix.

## Verification outcome

Final `./tools/build.ps1` passed all27 CTests, installed the EXE and rebuilt the ZIP after the signed pose/facial-copy fixes. The final private `local/enemy-targeting-signed-retail-check.tsv` scan passed205 rooms,318 floor trigger probes,10 personality view probes and640 cached root/body neck updates, with existing3328 auxiliary frames/126 manager updates and960 corridor ticks retained. Fresh ignored `local/enemy-targeting-runtime-ui-1` packaged smoke passed241 ticks/240 frames/53 movement ticks/10 focus blocks/menu return and INI checks. Format and diff checks passed. Source review found the signed initial neck issue; its regression and the facial signed-copy regression failed before fixes and passed afterward. Full root AI and the complete Step1 slice remain active work.
