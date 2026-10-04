# Native original room ambient implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; retain authorized inline execution.

**Goal:** decode original room ambient colors, execute command68 and consume
the current colors and ambient materials in native room/actor rendering.
**BASE:**4759efa. **Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** one current room ambient value belongs to ScenarioState.
Decoded room geometry carries its initial header value for standalone viewing.
Composed native frames retain draw-category selectors and current script state;
the rasterizer computes the real ambient component instead of the development
overview light. Source Ninja ambient material commands retain their RGB and
cross-model drawing state. Full direct lighting and audiovisual reference
acceptance remain required; this increment does not claim complete lighting.
**Tech stack:** C++17, software native rasterizer, CMake/Ninja/CTest, no SDK.

## Global constraints

- Full Step1 remains two connected rooms, original player/enemy gameplay,
  interactions/combat/inventory/sound/effects, versioned cross-room saves and
  independent original reference acceptance. New Game stays disabled.
- Windows native x64; no emulation, ELF/IOP or proprietary SDK execution.
- All checks headless, dummy devices and fresh ignored profiles. No desktop UI,
  audible output, browser, user profile mutation, publication or push/merge.
- Existing authorized checkout/inline workflow; exactly one fresh read-only
  gpt-6-astra/high reviewer, one author Critical/Important fix pass, no re-review.
- Original event.c bhEffAmbSet3666: six bytes68,R,G,B,index,padding, each source
  byte becomes0.1f*value, index0..3. Do not clamp script colors to1.
- ROM_WORK types.h1419: selectors at3B8..3BB, R/G/B arrays at3BC/3CC/3DC,
  four float32 values each; resolve header pointer from outer offset16.
- game.c bhAllDrawModel: room usesamb_rom, characters useamb_chr.
  objitm.c bhDrawObjItm: normal objects useamb_obj, half-light objects (id>=1000)
  and special object pass useamb_chr, items useamb_itm.
- ps2_NinjaCnk.c njCnkCmA/Da/As/Das: material ambient BGR bytes normalize255;
  room ambient multiplies material ambient. Direct lights/specular/fog/blend,
  complete effects and PS2 color equivalence are still required work.

## Review focus

1. Six-byte source cursor, exact float byte scaling including overbright colors,
   static four-slot bounds and missing/truncated state rejection before mutation.
2. Room selectors and float arrays decode from the console header without native
   pointer casts; malformed selectors/nonfinite values reject, signed finite
   colors retain source values until final raster channel saturation.
3. Live scenario colors, not decoded snapshots, reach real composed frames;
   transitions bind destination colors while retaining independent progress.
4. Room/character/normal-object/half-light-object/item categories select original
   ambient slots; source material commands and state across models/frames affect
   rendered pixels, not merely metadata, while standalone fixture viewing works.
5. Untouched retail entry probe advances68 without skips; decoded source/pixel
   and headless package evidence does not waive direct lighting, cemetery AI,
   computer139, audio/effects/combat/inventory/saves or original reference gates.

## Task1: Decode and original event command

Files: new src/ambient.hpp/.cpp and tests/room_ambient_tests.cpp; modify
scenario.hpp/.cpp, scene.hpp/.cpp, runtime.cpp, CMakeLists.txt.
Interfaces: RoomAmbient has four RGB float slots and four uint8 selectors;
AmbientTarget{Room,Character,Object,Item};
RoomAmbient decode_room_ambient(const Bytes&);
std::array<float,3> selected_ambient(const RoomAmbient&,AmbientTarget).

- [x] Write and execute original68 literal against BASE library. Expected RED:
  unsupported68; actual original six bytes must later advance to room-case55.
- [x] Add decoder/command tests for byte order, selectors, extent/index binding,
  nonfinite rejection, negative/overbright values, exact0.1f arithmetic and
  untouched other slots/flags. Write before implementation. Expected GREEN
  once decoder and command are implemented and bound before room initialization.
- [x] Bind decoded ambient to real room scene and current ScenarioState;
  destination entry replaces source ambient, composition rejects a missing
  required current binding rather than using a stale initial value.

## Task2: Real ambient rendering consumers

Files: ambient tests, scene.hpp/.cpp, camera_render.cpp, render.hpp, runtime.cpp,
runtime_view.hpp/.cpp. Interfaces consume Task1 RoomAmbient/AmbientTarget.

- [x] Write pixel regressions for selected color changes through command68 and
  composition, textured/untextured material multiplication, final saturation,
  invalid selected material/color state and source target categories. Test
  actual source A/Da/As/Das material chunks and persistence across model/frame
  drawing. Expected RED before rasterizer consumes original ambient/material.
- [x] Preserve material ambient state in decoded triangles/mesh end commands,
  apply it in source draw order and retain it per renderer session across
  frames. Bound rendering uses original ambient term; unbound development
  fixtures retain their independent overview presentation. Expected GREEN
  new cases plus existing camera/scene/room composition and entry tests.
- [x] Review fix: preserve source model spheres and pass clipping policy;
  reject source-culled models before material commands. Triangle/material-only
  behavioral RED, five focused ambient GREEN, full108/108 and package/probe.

## Task3: Original entry, verification and one review

- [x] Recompile unchanged retail entry probe; record selected ambient values,
  actual68 bytes and next missing original service without script changes.
  Expected real entry advances68 or exposes an actual asset/binding issue.
- [x] tools/build.ps1; fresh packaged dummy smoke; format/diff and exact
  six-file ZIP/hash checks. Expected full suite and native package pass.
- [x] Status/ledger and one fresh read-only review of all five focus items,
  original source and current increment including new files. Re-grade, fix
  Critical/Important once using RED-to-GREEN/full suite, defer Minors and rule
  on every declined judgment. Selected local commit; full goal stays active.
