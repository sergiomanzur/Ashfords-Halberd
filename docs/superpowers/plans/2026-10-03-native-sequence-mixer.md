# Native sequence mixer implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Execute both original graveyard ambient sequences into bounded native dry/effect-send PCM buses, with original instrument identity and sample loops.

**Architecture:** A portable sequencer schedules decoded SCEI events with integer rational timing. A native sample voice uses the original envelope words and four-sample interpolation, and a bounded mixer binds indexed instruments and retains separate direct/effect sends. No audio device is opened by this increment; these are the actual synthesis services needed by the subsequent room audio integration.

**Tech Stack:** C++17, CMake/Ninja, existing native sound decoders, headless CTest and private original assets.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.

## Global constraints

- "Never satisfy a required gameplay service with a silent no-op just to obtain a successful link."
- "The game executable is used for validation and research; it is not emulated or executed at runtime."
- All checks headless, no audible device, no normal user profile edits.
- Full Step1 remains the two-room playable slice, including combat, inventory, decoded audio, saves and original reference acceptance. Opcode148/New Game remain gated until runtime fades/effects/output are implemented.
- Decode original parameters; unsupported active modulation, velocity curves, pitch controllers and finite/nested loop forms report explicit errors. The selected ambient sequences use none of those forms.
- Preserve both dry stereo and effect-send stereo. Do not replace missing reverberation with a claim of complete original sound.
- Hardware output parity is unproven. Independently written DSP uses primary documented envelope/interpolation facts at48kHz, without copying GPL emulator implementation; reference comparison is still an acceptance requirement.

## Review focus

- Tempo changes, fractional frame deadlines and block partitioning must not change event timing or PCM.
- Malformed/zero-duration loops must fail instead of hanging; original endless loops must retain wall-clock time, release tails and channel state.
- Caller-owned bank lifetime, sparse programs and malformed parameter lengths must not permit dangling pointers or unchecked indexing.
- Oversubscribed voices and extreme render/event requests must fail within explicit bounds rather than allocate indefinitely.
- Sample loops must retain the initial pre-loop history and exclusive end; effect-only instruments must not leak into the direct bus.

## Task1: Rational sequence execution

Files: create `src/audio_playback.hpp`, `src/audio_timing.cpp`, `tests/audio_playback_tests.cpp`; modify `CMakeLists.txt`.

Interfaces: consumes `SceSequence`; produces `SceSequencer(SceSequence)` and `advance(uint32_t frames) -> vector<ScheduledSceEvent>` with absolute48kHz frame and original event index. Limit a block to48000 frames, returned events to65536 and source events to1000000.

- [x] Write tests before implementation: PPQN3 with tempo100000 at tick1 yields frame1600; tempo200000 at tick2 applies after frame3200; notes at tick3 arrive frame6400. PPQN7/tempo1 exercises fractional carry. Compare one block against arbitrarily partitioned blocks. Loop markers99=0/6=id and99=1/6=id/38=0 jump after the start ID without resetting time. Nonzero/nested/unmatched/zero-duration loops and backwards ticks reject.

```cpp
SceSequence seq{3, {{1,SceEventType::note_on,0,60,127,0},
                    {2,SceEventType::tempo,0,0,0,200000},
                    {3,SceEventType::note_off,0,60,0,0},
                    {4,SceEventType::end}}};
seq.events.insert(seq.events.begin(), {0,SceEventType::tempo,0,0,0,100000});
SceSequencer clock(seq);
auto events=clock.advance(10000);
check(events[1].frame==1600 && events[3].frame==6400,"tempo deadlines");
```

- [x] Compile/run RED: `cmake --build build/windows --target audio-playback-tests`; expected missing playback interface.
- [x] Implement checked delta*tempo*48000 rational arithmetic, ceiling deadlines, retained remainder, monotonic frame ranges and validated loop pair lookup. Tempo/channels are retained on a loop jump. Return source events (including loop controls); the mixer interprets sound controls.
- [x] Run `ctest --test-dir build/windows -R audio-sequence-timing --output-on-failure`; expected1/1.

## Task2: Native envelope and sample voice

Files: create `src/audio_voice.cpp`, `src/audio_gaussian.hpp`; extend playback header/tests/CMake.

Interfaces: `SoundEnvelope(uint16_t adsr1,uint16_t adsr2)`, `tick() -> int32_t`, `release()`, `active()`; `SampleVoice(shared_ptr<const SceSoundBank>, size_t wave, uint16_t pitch, uint16_t adsr1,uint16_t adsr2)` and `tick() -> int32_t`.

- [x] Write RED tests: attack/decrease/release, stopped/infinite sustain, documented fast exponential increase, fractional signed interpolation literal vectors, one-shot termination, loop history and immutable bank lifetime. Pitch is Q12, limited1..16384; wave extent/loop bounds/rate/attribute validated.

```cpp
SoundEnvelope envelope(0x000f,0x0000);
check(envelope.tick()==14336,"linear attack first step");
check(envelope.tick()==28672,"linear attack second step");
check(envelope.tick()==32767,"attack saturation");
envelope.release();
check(envelope.tick()==16383 && envelope.tick()==0,"linear release");
```

- [x] Compile/run RED for absent envelope/voice functions.
- [x] Implement independent integer envelope state, signed floor operations, original sample rate pitch, four-sample table interpolation and exclusive sample loop mapping. Hardware mathematical coefficients are transcribed from pinned primary psx-spx documentation. Do not add emulator dependencies or code.
- [x] Run `ctest --test-dir build/windows -R audio-native-voice --output-on-failure`; expected1/1, plus Task1 regression.

## Task3: Indexed instrument mixing and original asset proof

Files: create `src/audio_mixer.cpp`; extend playback header/tests/CMake; update `docs/native-audio-status.md`.

Interfaces: `SceMixer(shared_ptr<const SceSoundBank>,SceSequence)` owns immutable assets; `render(uint32_t frames) -> vector<SoundBusFrame>` where each frame contains int32 dry_left,dry_right,effect_left,effect_right. Maximum48 active voices and48000 frames per call. Instrument volume, additive pan, transpose/detune, sample rate, ADSR and sample sends bind from original blocks. Controllers7/10/11 update active voices; loop controls are consumed by the clock; unsupported controls/pitch reject.

- [x] Write RED tests using authored literal instruments: pitch/octave/rate, hard/center pan, effect-only routing, channel volume applied to existing voice, note-off release, layered voice cap, sparse/malformed instruments, and identical PCM across block partitions. All used parameter offsets must be checked. Check source limits and arithmetic overflow before allocations.

```cpp
SceMixer full(bank,sequence), split(bank,sequence);
auto pcm=full.render(1000);
auto a=split.render(17), b=split.render(983);
a.insert(a.end(),b.begin(),b.end());
check(a==pcm,"PCM independent of block partitions");
```

- [x] Compile/run RED before mixer implementation.
- [x] Implement indexed bindings and bus accumulation; reject active unsupported instrument branches. Retain wet sends as distinct data rather than discarding them.
- [x] Run `ctest --test-dir build/windows -R 'audio-(sequence-timing|native-voice|sequence-mixing)' --output-on-failure`; expected3/3.
- [x] Compile a private console audit against the production library and existing imported MULTSPQ1 key20/21. Render both bank3 sequences for36s (beyond each loop boundary), verify nonzero dry/wet buses, bounded peak voices and block partition equality; log duration, channel peaks, loop repetitions and digest, no audio device.
- [x] Run full `tools/build.ps1`, fresh packaged dummy-SDL smoke with SDL_AUDIODRIVER=dummy, formatting/diff checks. One fresh read-only reviewer for9711645..current diff. Grade findings against the spec; one TDD fix pass for Important/Critical, deferred Minors. Document exact evidence and unfinished runtime/reverb/reference boundaries. Commit only these changes, preserve goal active.

## Coverage audit

This is the required original sound synthesis dependency. Main-menu settings and import remain implemented; full campaign/modern camera are Step2. Combat, inventory, actual two-room traversal, SDL sound output/fades/reverb/ADX and saves remain required unfinished Step1 work; this plan does not redefine the goal as offline playback.
