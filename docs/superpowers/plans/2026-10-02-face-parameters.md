# Native facial keyframes implementation plan

> Use superpowers:executing-plans inline; authorized continuation of Step 1.

**Goal:** Decode and sample the normal MASK shape track needed by facial control.
**Architecture:** Owned native parameters replace PARAM_WORK pointers; source
interpolation modes and selective muscle updates retain their semantics.
**Tech stack:** C++17/CMake/CTest, headless Windows x64.
**Spec:** ../specs/2026-10-02-windows-port-design.md

Constraints: immutable retail bytes, no asset packaging, no desktop UI checks,
no enabling the incomplete graveyard controller.

Review focus: preceding variable tables affect key offsets; malformed extents,
non-finite parameters, duplicate frames, and interpolation/scaling semantics.

## Task 1: Normal MASK keyframe sampling

Files: src/face_parameters.hpp/.cpp, tests/face_parameters_tests.cpp,
CMakeLists.txt, tools/room_inspect.cpp, docs/actor-rendering-status.md.

Interface: decode_face_track(Bytes) returns owned FaceKeyframes;
sample_face_track(track, frame) returns FaceParameters.

- [x] Synthetic MASK includes a list, vertex references, connection, jaw and
      tongue tables preceding two shape keys. Run face-parameter-tests and
      observe the unimplemented decoding failure.
- [x] Decode bounded 164-byte normal PARAM_WORK keys, validating strictly
      increasing nonnegative frames and finite parameters.
- [x] Port linear, square, square-root, cosine and held interpolation from
      fmCnkSetInterParam; preserve unselected muscles, scale jaw/tongue and
      retain unscaled interpolated eye coordinates.
- [x] Port source frame selection/clamping; reject invalid frame inputs.
- [x] Verify synthetic boundary/interpolation tests and real graveyard track.
- [x] Run full CTests, rebuild Windows package, retail runtime/headless smoke.
- [x] Request read-only source review, commit and record evidence.

Packed lip parameter decoding, geometric facial deformation and MASK scheduler
control remain subsequent requirements; this task implements normal shape keys.

Verification: face-parameter-tests RED (decoder absent) -> GREEN.
Read-only review found analytic cosine differed from njCos. Reflected-angle
regression failed before correction and passed with ninja_sin_cos; all 21
CTest checks passed afterward. No review findings remain within this scope.
Final retail runtime check: local/facial-keyframes-final-runtime-check.tsv,
205 rooms, 960 joined ticks, selected MASK24 keys/frame46,93 half-frame samples
and exact authored muscle-key round trips. Package rebuilt successfully.
Fresh packaged smoke: local/facial-keyframes-runtime-ui-1,241 ticks/240frames,
53 movement ticks,10 focus-blocked ticks,menu return and INI verified.
No retail data is tracked or packaged. Complete Step1 remains active.
