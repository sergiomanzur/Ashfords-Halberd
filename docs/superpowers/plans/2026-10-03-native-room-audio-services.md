# Native room audio services implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Bind original room sound banks/environment and implement the source fades/depth ramp and native Hall effects stage needed by room playback.

**Architecture:** Decode each typed/indexed bank into immutable native assets with aggregate expansion caps. Keep source manager ticks separate from sample mixing. Process the existing dry/effect buses through an independently written, bounded Hall reflection/comb/all-pass network with39-tap rate conversion; never substitute a zero-depth-only renderer for the required effects service.

**Tech Stack:** C++17, native sound assets/mixer, CMake/Ninja, headless CTest.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.

## Global constraints

- "Never satisfy a required gameplay service with a silent no-op just to obtain a successful link."
- "The game executable is used for validation and research; it is not emulated or executed at runtime."
- All checks headless/no audible device/no normal profile edits. No PS2/IOP/SDK execution.
- Step1 remains the complete two-room slice with combat/inventory/audio/saves/reference acceptance.148/New Game remain gated until the sequenced services are actually joined to the room/audio runtime.
- Pinned upstream `ps2_sg_sd.c::sdSysServer` calls `SdrSetRev(core,5,CurrentFxLevel,0,0)` with a16-tick depth ramp. Wrapper `sdSndSetFxPrg`/`sdMidiSetFxLev` do not implement the actual effects. `RM_SNDENV` is16 bytes transported in a64-byte record.
- Preserve original MIDI fade arithmetic: `(((timer/100)*30)+(((timer%100)*6)/10))/2`, minimum1; timer120 means21 ticks, timer400 means60. Fade completion is on the tick after the counter reaches zero. Signed driver volume is truncated before adding128/clamping0..127.
- The independent effects DSP is a candidate requiring original PS2 reference acceptance; constants/format are hardware math, not copied emulator code. Actual Hall preset in open ps2sdk ac92a9f657d2e531dd8f060250b07f2a5ac6dea5 matches the documented SPU Hall coefficients. No ps2sdk/proprietary SDK runtime dependency is introduced.

## Review focus

- A duplicate bank/type or bank alias must not replace another asset silently or multiply decoded PCM/events beyond one aggregate pack budget.
- Source float/truncation and16-tick ramp endpoints must survive room-level changes without invented millisecond timing.
- Nonzero effect input with zero depth must preserve internal reverberation state for later depth changes, while output remains dry.
- Signed FIR/feedback products, extreme bus values and wraparound taps must avoid undefined overflow and stay bounded.
- Block partitioning/reset must not change DSP output or retain stale samples in a fresh processor.

## Task1: Room sound assets and source manager controls

Files: create `src/audio_room.hpp`, `src/audio_room.cpp`, `tests/audio_room_tests.cpp`; modify CMake.

Interfaces: `RoomSoundAssets decode_room_sound_pack(const Bytes&,size_t pcm_budget=16777216,size_t event_budget=1000000)` exposes separate16-index program/sequence/shot banks and raw16 signed environment fields. `MidiVolumeFade` exposes request/tick/driver_volume/active/stopped. `RoomEffectDepth` exposes request(int8_t),tick() -> int16_t and level().

- [x] Write RED tests: aggregate caps and duplicate identities from original decoder fixtures; no whole archive loading; signed environment fields/transport padding; source fade count/driver quantization/last tick;16-tick depth transition/endpoints. Preserve distinct bank identities.

```cpp
MidiVolumeFade fade;
fade.request(true,120);
check(fade.remaining()==21 && fade.driver_volume()==1,"source120 fade");
fade.tick();
check(fade.driver_volume()==8,"source signed volume truncation");
RoomEffectDepth depth;
depth.request(16);
check(depth.tick()==256,"source first depth step");
```

- [x] Run RED target; expected missing native room audio functions.
- [x] Implement bounded pack binding, exact float fade order and integer effect depth ramp, with explicit range/malformed errors.
- [x] Run focused room-audio tests; expected pass.

## Task2: Native Hall effect and48kHz bus output

Files: create `src/audio_reverb.hpp`, `src/audio_reverb.cpp`; extend room-audio tests/CMake.

Interfaces: `HallReverb::render(const vector<SoundBusFrame>&,int16_t depth)->vector<StereoPcmFrame>` and `reset()`. A22256-sample PCM16 circular buffer implements the required Hall preset offsets (hardware address words*4), IIR reflections,4comb terms and2all-pass stages.39-tap input FIR downsamples48kHz to24kHz; zero-insertion and the corresponding FIR restore48kHz. Signed floor multiplication and explicit saturation bound every stage; depth multiplies only the wet output. Render limit48000 frames.

- [x] Write RED tests: zero input/reset, clipped dry-only bus literal vectors, independent FIR impulse coefficients, delayed nonzero/cross-channel Hall tail, original zero and16 depth, exact block partition equality, silent depth preserving state and over-limit blocks.

```cpp
HallReverb reverb;
auto output=reverb.render({{40000,-40000,0,0}},0);
check(output[0]==StereoPcmFrame{32767,-32768},"bounded dry output");
```

- [x] Compile/run RED for absent DSP behavior.
- [x] Implement the complete required Hall network and filters; do not discard the input/send buses or reset feedback on a mere depth change. Keep unknown preset support explicit rather than infer it from RoomFxProg, which source ignores here.
- [x] Run focused Hall/room/fade tests; expected pass.

## Task3: Original-asset joined proof and verification

Files: update `docs/native-audio-status.md`; private audit under ignored local.

- [x] Decode keys10/20/21 through production pack binding; confirm environment depths16/0/0, all typed bank identities and original bank3 sequences0/1.
- [x] Join production SceMixer with fade/depth/effect services for36s of both sequences in keys20/21, with the actual zero-depth environment and a separate nonzero-depth stress/reference candidate. Check nonzero dry output, delayed nonzero wet response, exact PCM partitions, finite bounded output and reset. Never claim nonzero-depth override is authored graveyard behavior.
- [x] Full `tools/build.ps1` and fresh packaged dummy smoke, current original entry boundary probe, ZIP contents, format/diff checks. Expected full suite exit0; entry still explicitly rejects148 until subsequent runtime integration.
- [x] One fresh read-only reviewer for this increment; one TDD fix pass for Critical/Important only, record deferred Minors and rulings; local commit. Keep goal active and full acceptance scope intact.

## Coverage audit

These are required services for the next native148/two-slot/SDL integration. This does not complete game playback, ADX music/voice, combat, inventory, saves or original-reference acceptance, and does not reduce their scope.
