# Original room lighting implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan inline. Steps use checkbox syntax for tracking.

**Goal:** Execute original room light events through the native controller and authored-normal/material pixel pipeline, including drawing-group light changes.

**Architecture:** Decode LGT_WORK explicitly into owned native work, separate the original fixed-tick controller from its drawing snapshots, and carry retained material updates through model clipping and rasterization. Command53 is enabled only when its native controller and pixel consumer are connected. Drawing mutations publish after a valid frame.

**Tech Stack:** C++17, CMake/Ninja, existing SDL2 Windows runtime, Python for private independent reference capture. No console SDK or emulation in the runtime.

**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md

## Global constraints

- Players provide their own SLUS-20184 ISO; no retail data enters Git or distribution.
- All checks use dummy SDL video/audio/software, fresh ignored user profiles and no desktop windows.
- Keep the pinned upstream checkout unmodified as reference.
- Never satisfy a required gameplay service with a silent no-op just to obtain a successful link.
- Full Step1 still requires two connected actual rooms, gameplay, combat, inventory, decoded audio/effects, versioned cross-room saves and independent original-game acceptance. New Game stays disabled.
- This increment includes full light selection, links, dynamic types, shared RNG, materials, per-vertex shading and drawing policy; it is not a flag-only light instruction.
- Preserve the current checkout and previous commits. Work locally on codex/windows-foundation; no push/merge/publish.

## Source decisions and evidence

LGT_WORK stride224: flag0/type4/speed8/link12/index16/joint20/source24,
position28/local40/vector52, flicker accumulators64..72, RGB76..84,
near88/far92/angles96..112/mode116/counters120..132,
cached position136/vector148/accumulators160/RGB172/ranges184/angles192.
ROM normal pointer4/count132; event pointer60/count180. Normal init clears
bit2 in the first four and zeroes mode/counters. Event caches stay authored.
CAM_WRK light clip is byte2; eight hide words start152. Cmd53 has four bytes:
mode0 enables bit1; other modes disable; table0 normal, other tables event.

PS2 chunk51 executes VU0_CALCPOINT, not its unused scalar fallback.
Its MAC signs captured at0260/0268/0270 correspond to dot/near/far after
four cycles. The point branch skips negative dot, distance beyond far and
distance inside/equal near; its delay slot still writes the unattenuated
dot before the inside-near branch skips attenuation. Therefore inside near
retains full diffuse, outside near applies nearSquared/distanceSquared.
The delay slot at0328 is essential. Equal far is included by its sign test.
The scalar fallback's strict far comparison is different; do not silently
use that as PS2 boundary evidence. Branch sign interpretation is checked
against primary PCSX2 VU pipeline source; this is source analysis, not hardware
acceptance or a runtime emulator dependency.

Material commands17..23 update retained diffuse/alpha, ambient, specular
and exponent; command3 sets exponent and consumes four bytes; command16 uses
njCnkDefaultShort, which consumes a four-byte header plus usSize 16-bit words.
Untextured strips always use the actual diffuse material. Textured Easy
uses white diffuse, Simple uses retained diffuse; multi specular is white,
single uses retained specular. Source color tables and literal 17 multiply
specular calculation are retained. GS texture modulation divides by128;
fog divides by256, including disabled-fog vertex F255. Flat strips use the
last submitted vertex. Clip generated attributes before screen interpolation.

## Review focus

- Event/normal table switching with active reserved lights and inactive linked targets must preserve selection and RNG timing.
- Cached work when SP80 is clear must not advance dynamic counters, but links still follow current matrices.
- Material-only, hidden and clipped meshes must obey original retained-state ordering without publishing state from invalid frames.
- Equal point ranges, negative/bright colors and camera screen-margin boundaries must use source branches without arbitrary fallbacks.
- GM80 flash changes on multiple drawing calls and item highlight must preserve single/multi channels and publish valid draw counters.

### Task 1: Native light work and source control

**Files:** Create src/vector.hpp, src/light.hpp, src/light.cpp, src/light_control.cpp; modify src/scene.hpp, src/camera.hpp/.cpp, src/scenario.hpp and CMakeLists.txt; test tests/light_tests.cpp.

**Interfaces:**
- Consumes: ScenarioState, scenario_random(ScenarioState&), ScenarioEntity model_matrix/model_work, NativeEffectPool model_matrix and authored-slot maps, Camera/CameraDefinition.
- Produces: RoomLights decode_room_lights(const Bytes&); void initialize_room_lights(RoomLights&); void select_light_cut(RoomLights&,const CameraDefinition&); NativeLighting control_lights(RoomLights&,ScenarioState&,const ScenarioEntity*,const Camera&,const CameraDefinition&,uint32_t camera_flags); NativeLighting drawing_lights(RoomLights&,const NativeLighting&,bool easy,bool half).
- RoomLights owns normal/event vectors. LightWork preserves flags/type/speed/link/source/base/cached values/mode/counters. NativeLighting owns directional, up to three points and single intensity/ambient/vector/color, plus original direction presence.

- [x] Write behavioral BASE tests. Use __has_include("light.hpp") to report missing controller as a test failure, not a compiler error. Check actual53 bytes53,1,4,0 fail before implementation; mode/table/index/extent validation and continuation marker are covered.
- [x] Run `g++ -std=c++17 -O2 -I src tests/light_tests.cpp build/windows/libashford_assets.a -o local/light-baseline-tests.exe`; run each mode and redirect to local/light-baseline-red.log. Expected: behavior failures for absent native light consumer.
- [x] Implement endian decoding with finite consumed floats, overflow-safe spans, counts <=256 per table, preserving disk work caches and ignoring console pointer fields. Decode CAM hidden words and clip byte. Init and cut selection operate only on normal work in original order.
- [x] Port light.c dynamic switch0..13/100/101 using ninja_sin_cos and shared scenario_random; use explicit wrapped 32-bit counters. Resolve linked matrices with checked authored slot maps; inactive work clears flags3 and skips cache publication. Select first directional and first three in-view points; support normal/event/reserved selection and SP80 cached work.
- [x] Verify layout, control, linked, dynamics modes with CTest. Expected: all named cases pass, including shared RNG sequences and source work comparisons. Do not enable scenario53 yet.

```cpp
RoomLights lights; lights.normal.resize(4);
lights.normal[0].flags = 3; lights.normal[0].type = 100;
initialize_room_lights(lights);
ck(lights.normal[0].flags == 1 && lights.normal[0].mode == 0,
   "normal reserved lights reset before room events");
```

### Task 2: Retained materials and original native shading

**Files:** Create src/light_shading.cpp; modify src/scene.hpp/.cpp, src/render.hpp, src/camera_render.cpp; tests/light_tests.cpp and tests/light_reference_fixture.hpp. Private capture helper stays under local/.

**Interfaces:**
- Consumes: NativeLighting, authored Corner::normal, Camera, draw mode0..3 and RoomAmbient selection.
- Produces: LightMaterial, LightMaterialUpdate, apply_light_material(LightMaterial&,const LightMaterialUpdate&), light_vertex_color(const NativeLighting&,const Camera&,Vec3 position,Vec3 normal,const LightMaterial&,const std::array<float,3>& ambient,uint8_t draw_mode,uint8_t strip_flags,bool textured).
- Triangle owns optional cumulative material fields/strip flags; Mesh ending material updates preserve commands after final strip. Scene/Mesh lighting snapshots distinguish the actual native runtime from synthetic legacy fixture shading. render_camera gains optional retained full LightMaterial pointer, publishing only valid frames.

- [x] Write source material/shading and raster tests before implementation: textured/untextured mode tables, multiple retained commands, single/multi, normal direction, point near/far, clipped unequal vertex colors, flat provoking vertex, GS texture/fog order and occlusion. Run focused modes. Expected: missing native pixels/material failures.
- [x] Capture numerical reference independently from pinned source vertex-color assembly operations and record source hashes. Include dot/range boundary fixtures from the actual VU point branch, including delay-slot behavior; do not label scalar capture hardware parity.
- [x] Decode command3's four-byte header and default16's size-counted short payload, and material17..23 correctly; update only present material components. Store polygon head bits. Validate consumed color inputs and exponent. Test command-only meshes and ending state.
- [x] Compute source light intensity from authored normals and camera transforms, followed by source color table. Preserve float arithmetic, light order, literal pow17 and byte conversion. Carry RGB/alpha/fog through clipping and screen-linear RGB interpolation; UV stays perspective-correct. Texture modulates /128, fog applies F/256 even when F255, depth and transparency obey strip behavior.
- [x] Run `ctest --test-dir build/windows -R native-light --output-on-failure`. Expected: every implemented shading/material/raster case passes. Strengthen deferred direct skin level9 endpoint coverage within this original-render validation scope.

```cpp
auto color = light_vertex_color(lighting, camera, {0,0,-3}, {0,0,1},
                                material, {0,0,0}, 1, 0, true);
ck(color[0] == 128 && color[3] == 128,
   "original vertex intensity and alpha reach the GS color range");
```

### Task 3: Original room/runtime/event/draw integration and acceptance

**Files:** Modify src/scenario.cpp, src/runtime.hpp/.cpp, src/runtime_view.hpp/.cpp, src/scene.cpp; tests/light_tests.cpp; create docs/native-room-lighting-status.md.

**Interfaces:**
- Consumes: control_lights/drawing_lights and render_camera full retained material.
- Produces: live optional ScenarioState::lights and NativeLighting; void control_room_lights(RoomRuntime&); Scene ending draw light state for RuntimeView publication after successful rendering.
- Room entry binds/initializes light work before initialization events and applies current cut hide mask; clocks first advance in bhMainSequence at the camera boundary on fixed ticks. Composition copies work; no const_cast of live runtime.

- [x] Write runtime/event/drawing tests:53 changes actual composed pixels, malformed53 is transactional, room immutable data stays unchanged, fixed-tick updates, source normal/event flags, full/half object groups, unconditional restore before items/main actors, item highlight overrides multi direction/color only, GM80 easy intensity/flash multiple call timing. Run focused modes. Expected: missing53/native runtime lighting/drawing behavior failures.
- [x] Enable53 with checked bound work after controller/shader exist. Connect fixed-tick runtime and cut binding. Bind actual room scenes to native lighting; no geometric/ambient placeholder may substitute for the runtime light state. Preserve unsupported original dynamic camera guard until its real controller is implemented.
- [x] Implement drawing policy from game.c/objitm.c: room current state, full normal objects unlessST20, half groups unlessST20, restore full before items unlessST20, half before main enemy/player pass; GM80 draw-mode mapping and easy light calls, highlight's multi-only override. Publish the copied draw work after valid frame.
- [x] Run `tools/build.ps1` with output redirected to local/lighting-full-build.log. Expected: all existing123 and new tests pass; install and ZIP complete. Run fresh UUID native_runtime_smoke.py profile and verify_room_ambient_package.py. Expected: dummy playback/movement/focus/INI pass and exact six-file package matches installed EXE.
- [x] Compile/run unchanged room-entry probe and a private retail lighting audit. Expected: corridor production entry succeeds; original53 is consumed in cemetery with the next genuine unsupported controller identified. Audit original events/cuts/pixels/RNG without script edits or forced flags. Remaining full Step1 gates stay explicit.
- [x] Normal `git diff --check`, formatting and distribution checks; update status and ignored ledger with exact evidence. Expected: clean checks, no retail files staged.
- [x] Dispatch one fresh read-only gpt-6-astra/high reviewer on the entire increment BASE54d0f73, then regrade findings; one Critical/Important RED→GREEN fix pass plus full suite; defer Minors, ledger every declined judgment. No re-review. Result:4 Important accepted and corrected,1 Minor deferred, all9 declined judgments ruled in local/native-room-lighting-review-rulings.md. Four authentic RED cases now pass with17 focused and140 total tests; actual VSM source capture supersedes the earlier inline color fixture.
- [x] Commit completed increment locally with explicit paths and message `Port original room lighting through native rendering`. Full Step1 goal remains active until its complete gate passes.

```cpp
run_scenario(Bytes{53,1,4,0,55,7,0}, runtime.scenario);
control_room_lights(runtime);
ck(runtime.scenario.room_case == 7,
   "original four-byte light command preserves following instruction");
```
