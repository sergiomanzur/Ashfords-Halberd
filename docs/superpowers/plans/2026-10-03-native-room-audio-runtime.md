# Native room audio runtime implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; approved inline execution.

**Goal:** Execute original background sound requests through room initialization,
native two-slot synthesis and bounded SDL output, with a working main-menu/INI
effects volume. Preserve the complete Step1 goal and its unfinished acceptance.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`.

**Architecture:** Scenario audio state contains bounded value requests, never
device/mixer ownership. Original148/146 schedule requests; source command3 starts
initialization, command4 leaves its flag intact, command1 starts playback after
assets resolve. RoomRuntime stores immutable assets selected by source key
stage*1000+room*10+case, consuming/resetting the source case. RuntimeView owns
two-slot native playback and SDL device lifetime. Existing native60Hz manager
ticks produce800 PCM frames at48kHz. Keep depth/fades/feedback persistent during
playback, and stop/clear the device on leaving the preview or errors.

## Global constraints

- Headless checks only, SDL_AUDIODRIVER=dummy, no normal profile, GUI or audible output.
- No silent unsupported gameplay/audio branches, no IOP/SDK/ELF execution.
- Candidate DSP/linear driver gains are not proven PS2 parity.
- Music/voice controls wait for their actual ADX implementation. Only working
  effects volume is exposed now; all implemented controls live in main Options.
- Native148 support must advance actual room entry; a synthetic shortcut alone
  is insufficient. Newly exposed unsupported instructions remain explicit and
  inform the next required increment, never redefine full-slice completion.
- Full combat/inventory/two-room travel/saves/reference checks remain required;
  New Game stays disabled until complete.

## Review focus

- Scenario copies and failed destination loads cannot mutate a live audio device.
- Source raw-code comparison, forced bank3, initialization/fade-in ordering,
  independent slots and stop scheduling are preserved; no immediate148 play.
- Bad bank/sequence/slot/timer rejects before applying a batch; memory/voices,
  rendered frames and queued SDL bytes stay bounded, failures stop playback.
- Device/menu/focus lifetime prevents queued audio continuing after preview exit.
- Effects volume persists with value validation and changes actual output;
  zero volume preserves native sequence clocks and DSP state.

## Task1: Original requests and two-slot native playback

Files: create src/audio_background.hpp/.cpp, src/audio_runtime.hpp/.cpp,
tests/audio_runtime_tests.cpp; modify scenario.hpp/.cpp and CMake.

- [x] RED literal tests:148/146 sizes, invalid slots/truncation, raw code comparison,
  initialization still active after finish, forced0x300 and120timer on fade-in,
  copy isolation/bounded commands; two distinct slots, source fade/stop timing,
  immutable bank ownership, mute clock continuity, invalid batch preservation.
- [x] Run RED; expected missing native request/playback interfaces.
- [x] Implement value requests and two-slot SceMixer/fade/depth/Hall processing,
  independent gains and bounded combined buses, source commands and fail-terminal
  playback on render errors. No device access in scenario evaluation.
- [x] Focused GREEN tests; expected pass.

## Task2: Room binding, SDL output and in-menu persisted effects volume

Files: runtime.hpp/.cpp, runtime_view.hpp/.cpp, new audio_output.hpp/.cpp,
main.cpp, core.hpp/.cpp; new SDL tests plus core/packaged smoke verification.

- [x] RED: resolver uses post-script case and preserves failed replacement;
  dummy device PCM format/byte counts, bounded queue, pause/clear/destruction;
  effects INI valid/invalid/backup roundtrip and zero-volume output.
- [x] Implement post-init asset resolver/fade-in requests, mandatory sound binding
  for requested audio, RuntimeView synthesis/output ownership. Queue stereoPCM16
  at48kHz with a bounded half-second backlog and explicit device errors.
- [x] Add Effects volume to main Options, persist atomically and apply output gain;
  existing display/menu/viewer controls remain reachable. Clear/pause on preview
  exit/error/focus loss; render clocks remain managed by the current runtime.
- [x] Focused GREEN tests and dummy packaged smoke, expected pass.

## Task3: Real entry proof, full verification and one review

- [x] Bind original keys10/20/21 from selected AFS extents via production resolver;
  run actual initialization scripts.148 must no longer be the entry boundary;
  document any next unsupported operation without skipping it.
- [x] Run clearly isolated actual148 requests for36s per original graveyard
  variant while full entry remains blocked47/67; both request only slot0.
  Verify two-slot playback separately in native fixtures, and original request
  nonzero/mute-clock behavior and bounded dummy transfer. This diagnostic cannot
  establish successful graveyard initialization or gameplay acceptance.
- [x] Full tools/build.ps1, fresh packaged profile smoke, ZIP/format/diff checks.
- [x] One fresh read-only review for this increment; one RED→GREEN fix pass for
  Critical/Important, defer Minors, ledger all rulings, local commit only.

## Remaining acceptance

This integrates original ambient audio requests, not the complete audio system.
ADX music/voices, shot/footstep/enemy sound scheduling, original bank3 reuse and
room-change fade-out orchestration still need required source integration where
the slice uses them. Enemy gameplay, combat, inventory, actual cross-room travel,
versioned saves and independent original-game acceptance remain part of Step1.
