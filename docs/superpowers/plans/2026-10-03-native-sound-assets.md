# Native original sound asset decoding implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline with TDD. One fresh read-only review after the complete increment.

**Goal:** Decode the actual selected rooms' SPQ/SCEI banks, original ADPCM
samples and MIDI event lists into bounded native structures, and verify PCM
against an independent headless decoder.

**Architecture:** The portable asset library owns disk decoding. A console
inspector reads the already imported AFS/IDX, writes fresh private WAV/TSV
diagnostics, and never opens an audio device. vgmstream is a development-only
oracle. This is an audio dependency of the full playable slice; no gameplay
opcode is enabled until its real playback service exists.

**Tech Stack:** C++17, existing CMake/Ninja/GCC, Python verification,
vgmstream r2117 Windows x64 CLI (SHA256 pinned).

**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md;
docs/step1-room-selection.md.

## Global constraints

- Native Windows code; imported user-owned USA assets only.
- All validation headless; no audio output, dialogs, GUI or desktop access.
- Keep New Game disabled and opcode148 explicitly unsupported until playback.
- No private game bytes in source, tests, Git or packages.
- Preserve song indices, sparse program indices, timing and sample loops.
- Unknown format/event/compression must fail explicitly, never silently skip.
- Reuse authorized codex/windows-foundation checkout. No push/merge/publication.

## Review focus

1. Truncated/overflowing headers must reject before allocation or pointer use.
2. HD/BD transport padding differs from the declared bank length in real files.
3. Sparse instrument tables must retain their original indices and references.
4. Sequence running status and high-bit delta omission differ from ordinary SMF.
5. ADPCM terminal padding must not become audible; loop bounds must survive.

## Task1: native ADPCM sample decoding

Files: src/audio_assets.hpp,src/audio_adpcm.cpp,tests/audio_asset_tests.cpp,
CMakeLists.txt. Produces PsAdpcmSample decode_ps_adpcm(const Bytes&).

- [x] Write/run literal sample checks (low-nibble first, negative signs,
  predictor history across blocks, clipping, end and loop bounds). The first
  assertion expects28 decoded samples, first values1,-1,7,-8 from header0x0c.
  RED: fail the deliberately empty decoder, before implementing it.
- [x] Implement bounded16-byte/28-sample decoding with rational predictor
  coefficients, explicit invalid headers, terminal flag7 exclusion, first
  end marker and optional loop start/end. Expected: audio-asset-decoding passes.
- [x] Add malformed/truncated input and unsupported header fixtures. Verify
  they reject rather than reading padding or silently substituting coefficients.

## Task2: SPQ/SCEI bank and sequence structures

Files: src/audio_assets.hpp,src/audio_assets.cpp,src/audio_sequence.cpp,
tests/audio_asset_tests.cpp. Consumes Bytes/PsAdpcmSample; produces
sound_pack_index, sound_pack_entries, decode_sce_sound_bank and
decode_sce_sequences. SceSoundBank retains samples/layers/sets/programs by
original index; SceSequence retains timed events including loop controls.

- [x] Add literal SPQ fixtures for key lookup, sentinel, extent/header overlap,
  unknown type, bank range and missing terminator. RED before implementation.
- [x] Add small hand-authored HD/BD fixture with two indexed waves, sparse
  program, sample set, split and ADSR parameters. Reject escaping chunk offsets,
  bad cross references, invalid metadata and a truncated wrapper. RED then GREEN.
- [x] Add SQ fixture with tempo500000/PPQN480, two sequence indices, note-on,
  source one-byte note-off, running status, delta omission and preserved loop
  control events. Unknown status, missing end, malformed VLQ and unsupported
  compression reject. RED then GREEN.
- [x] Read original key10/20/21 banks with the production parser. Expected:
  complete decoded banks and both graveyard ambient sequence indices; record
  counts and next real runtime boundary without changing scenario expectations.

## Task3: headless inspection and independent PCM verification

Files: tools/audio_inspect.cpp,tools/setup-audio-oracle.ps1,
tests/audio_oracle_check.py,CMakeLists.txt,licenses audio notices,
docs/native-audio-status.md,docs/tooling-status.md.

- [x] Inspector usage: AshfordAudioInspect MULTSPQ.AFS MULTSPQ.IDX room-key
  new-output-directory. Uses production parsers; writes WAV plus manifest and
  sequence TSV. A second invocation must reject an existing directory. No SDL.
- [x] Install optional oracle into ignored local/tools after SHA256 verification:
  vgmstream-win64.zip r2117,
  6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c.
  Validate zip path containment; no game runtime dependency is introduced.
- [x] Compare each exported PCM prefix against vgmstream CLI -i/-s decoding
  of the original HD/BD files, allowing only documented quantization differences
  measured in the final report. PCM must be nonzero for actual sound samples.
- [x] Run full tools/build.ps1, fresh packaged dummy-SDL smoke and format/diff
  checks. Expected: all existing tests plus the audio checks pass, EXE/ZIP fresh.
- [x] One fresh source review; fix only material findings with one RED/GREEN
  pass/full suite. Record all rulings and deferred Minors. Local commit only.

## Completion limits

This plan completes native sound **asset decoding**, not sound playback or
Step1. Sequenced synthesis, envelopes/mixing/reverb/fades, original ADX audio,
SDL output and script/runtime integration remain required. Actual graveyard
entry still fails148 until those services are implemented.

## Completed evidence

All tasks executed inline from2e2e83e. Literal RED/GREEN fixtures establish
codec, pack, bank, sequence and inspector behavior; one fresh read-only review
identified aggregate alias allocation amplification. One fix pass added
tighter caller budgets plus hard PCM/event caps, with three separate observed
RED/GREEN cases and77/77 final headless tests.

224 actual nonzero waves/6141884PCMframes from keys10/20/21 match vgmstream
exactly after the fix. Common.MLT decodes11waves. New EXE/ZIP contain notices
and no proprietary assets/oracle executable. Fresh packaged smoke and actual
room-entry probes passed their dependency checks, retaining the explicit148
runtime block. Deferred Minor: inverted note-range semantics/coverage before
instrument note-selection synthesis.
