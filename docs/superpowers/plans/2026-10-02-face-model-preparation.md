# Facial model preparation implementation plan

> Use superpowers:executing-plans to implement this prerequisite inline.

**Goal:** Prepare the original graveyard ID 91 model for the native facial renderer.

**Architecture:** Convert ordinary skin in a private model before expanding the face hierarchy. Preserve immutable facial source geometry and separate mutable destinations; use explicit FM skin layout after expansion. This supplies preparation, not facial animation or enemy control.

**Tech stack:** C++17, Windows x64, CMake/CTest, existing native model renderer.

**Spec:** ../specs/2026-10-02-windows-port-design.md

## Constraints

- Native game code; no emulation and no packaged retail assets.
- Headless checks only, with isolated local profiles.
- Follow face_bh.c, face.c and njplus.c from pinned upstream.
- Do not enable unsupported controller paths or describe this as a playable slice.

## Review focus

Malformed MASK extents/IDs, overlapping skin outputs, clone search ordering,
ordinary versus expanded skin indexing, and double application of world position.

## Task 1: Prepared face model and FM rendering

Files: src/face_model.hpp/.cpp, src/actor.hpp/.cpp, src/actor_render.cpp,
tests/face_model_tests.cpp, CMakeLists.txt, tools/room_inspect.cpp.

Interface: `PreparedFaceModel prepare_face_model(const ActorModel&, const Bytes&)`.
The result owns the expanded actor, original-to-runtime joint indices, source and
destination facial buffer offsets, facial part bindings and initial O_WORK.

- [x] Write synthetic source hierarchy and MASK fixtures. Assert 12 -> 16 joints,
      retained original child chains, private facial outputs, and body placement
      `(111,200,300)` for actor position `(100,200,300)`.
- [x] Run `face-model-tests`; expect explicit unimplemented preparation failure.
- [x] Preconvert standard skin against translation-only original bind positions;
      preserve level and auxiliary fields; reject shared writable vertex extents.
- [x] Port model-bearing DFS search and tooth/tongue/eye wrappers. Copy face and
      tongue descriptors/vertex chunks; flatten into a new native-owned MDL table.
- [x] Port jaw anchor angle initialization with source atan ratios; initialize
      O_WORK flag 4 on runtime joints 6..14.
- [x] Decode expanded FM skin by skipping runtime 11..14 and adding 4 to packed
      influences above 10; do not apply bind conversion a second time.
- [x] Run boundary tests for malformed signature, absent/duplicate parts,
      invalid graph/vertices, invalid mapped skin binding, and immutable inputs.
- [x] Probe retail entry 2 slot 4: expect 27 -> 31 joints and finite FM output.
- [x] Run `./tools/build.ps1`, 205-room runtime probe, packaged headless smoke.
- [x] Request read-only source-fidelity review; address substantive findings.
- [x] Commit explicit source/test/docs files and record verification evidence.

The next prerequisite is facial deformation and MASK control before ID 91 can
participate in the original enemy scheduler. Step 1 also still needs combat,
audio and cross-room saves.

Verification: all 20 CTests passed; Windows executable and ZIP rebuilt.
Final retail runtime probe: local/facial-preparation-final-runtime-check.tsv,
205 rooms, 960 joined ticks, selected ID 91 expanded 27 -> 31 with six meshes.
Packaged smoke: local/facial-preparation-runtime-ui-1, 241 ticks/240 frames,
53 movement ticks, 10 focus-blocked ticks, menu return and INI verified.
Read-only source review found no substantive defect; its jaw/rotation test
recommendations are covered. An additional RED -> GREEN guard rejects FM
rendering when preconversion is missing. No retail bytes or user profiles are
tracked or packaged. Step 1 remains active and incomplete.
