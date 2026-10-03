# Native facial lipsync implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this focused increment inline, task by task.

**Goal:** Port bounded packed lip shape decoding and the original two-tick mouth playback dependency required by Step 1's graveyard ID 91 controller.

**Architecture:** Own decoded lip shapes and clip segments in native structures. Preserve signed integer angle words until interpolation, retain unselected facial muscles and eye coordinates, and commit parameters/playback state only after successful evaluation. This component implements `fmSetLipSyncParam` and the two Lip parameter setters; room resident binding and `bhControlMask` scheduling follow separately.

**Tech Stack:** C++17, CMake/Ninja, existing native facial parameters/Ninja math, headless CTest.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64 code derived from the pinned USA decompilation.
- Keep the pinned upstream checkout unmodified as reference.
- Treat console offsets, pointer widths, alignment, and endian rules explicitly.
- No retail assets in Git or distributions; all checks headless.
- This increment does not complete Step 1 or enable New Game.

## Review focus

- Packed angle words may look like NaN floats: decode as signed integers.
- Short clips and odd durations preserve one transition per call and carry ticks.
- Sentinel resets to neutral shape zero, including a clip with no segments.
- Only selected muscles, jaw and tongue change; eyes and other muscles survive.
- Truncated/invalid clips and late interpolation overflow preserve caller state.

## Task 1: Packed shapes and lipsync playback

**Files:** Create `src/face_lipsync.hpp`, `src/face_lipsync.cpp`, `tests/face_lipsync_tests.cpp`; modify `CMakeLists.txt`, `docs/actor-rendering-status.md`.

**Interfaces:** `decode_lip_shapes(const Bytes&)` produces native `LipShape` values with raw signed angle words. `start_lip_playback(const Bytes&, size_t)` validates byte pairs and terminating 0xff into owned segments. `tick_lip_playback(FaceParameters&, const vector<LipShape>&, LipPlayback&, uint32_t)` evaluates the source ordering transactionally.

- [x] Write fixtures with 164-byte packed PARAM_WORK records, clips `{1,12,2,4,255}`, masked muscles, retained eyes, signed angles and known literal outputs. Cover all interpolation modes and short/odd clips, termination, malformed input and unchanged state on failure.
- [x] Configure/build the test and observe RED because the lipsync API is absent.
- [x] Implement bounded decoding, source integer-angle arithmetic and playback transitions. Reuse ordinary interpolation for selected float fields, overriding packed angle interpolation and restoring eyes.
- [x] Run targeted lipsync tests, then `./tools/build.ps1` for the full headless suite/package.
- [x] Run the existing private retail room probe and fresh-profile packaged runtime smoke, record evidence and remaining controller work, request source review, fix material findings, and commit.

## Source evidence

`upstream/recvx-decomp/src/ps2/veronica/prog/face_bh.c`: `fmSetLipSyncParam`, `fmCnkSetParamLip`, `fmCnkSetInterParamLip`. `face.h`: LIP_WORK. `face.c`: caller mask `0x03fff000`; `room.c`: resident lip pointer is read from the room's motion header, not SYSTEM.AFS.

## Task 2: Resident room lip clips

**Files:** Modify `src/face_lipsync.hpp/.cpp`, `tests/face_lipsync_tests.cpp`, `tools/room_inspect.cpp`.

**Interface:** `decode_room_lip_clips(const Bytes&)` returns owned length-prefixed clip records in original index order, including empty records. Null motion/lip pointers return an empty bank. The containing extent ends at the next room section/motion pointer.

- [x] Add a synthetic room with two enemy pointers, room motion, mask and lip pointer slots, indexed clips and the bank sentinel. Check absent pointers, empty clip indices, overlap, truncated extents and missing bank sentinel.
- [x] Observe RED before declaring/implementing the room clip API.
- [x] Implement bounded pointer-table and bank decoding; preserve original indices, allow padded bytes after a bank sentinel, and cap native record count at 65536.
- [x] Extend the retail headless room probe to decode all resident banks and validate every nonempty clip, then run the full suite and fresh packaged smoke.

## Verification and rulings

- RED: targeted compile failed on missing `face_lipsync.hpp`, then on undeclared `decode_room_lip_clips` before each implementation. Targeted tests passed after implementation.
- Corrected a test's approximate cosine value to 4.3820109 using the source integer angle 6553 and reflected sine-table index 9830; implementation retained the existing source lookup.
- `./tools/build.ps1`: full Windows build, 23/23 headless CTests, installed executable and ZIP passed after final code/test changes.
- Retail runtime probe: 205 rooms, 4,363 scenario entry slots; 617 lip clips in 44 rooms, 12,042 segments; existing 960 joined runtime ticks and 93 facial deformation samples retained.
- Packaged fresh-profile smoke: 241 ticks, 240 frames, 53 movement ticks, 10 focus-blocked ticks, main-menu return and persisted INI.
- Source review approved playback and resident decoder with no important findings. The suggested late-overflow regression now checks every parameter, playback scalar and segment.
- Ruling: reject packed angle differences outside signed 32-bit range rather than relying on C signed overflow. This explicit malformed-input limit has no observed retail stream implication; shape-bank binding remains pending.
- Ruling: room clip loading is a separate bounded decoder, not an implicit scenario actor change. Gameplay must still implement six-shape group binding, MASK lifecycle/gating and runtime eye-angle copies.
- Step 1 remains active and incomplete; New Game remains disabled. No visible windows or ordinary user profile were used for verification.
