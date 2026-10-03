# Native facial deformation implementation plan

> Use superpowers:executing-plans inline for this authorized Step 1 dependency.

**Goal:** Apply the selected graveyard MASK parameters to native geometry.
**Architecture:** Decode bounded native bindings; stage model writes and commit
only after muscle, jaw, tongue and eye calculations succeed. Preserve immutable
source geometry and the original mode-1 normals.
**Tech stack:** C++17, CMake/CTest, headless Windows.
**Spec:** ../specs/2026-10-02-windows-port-design.md

Constraints: native code, unchanged retail input, no packaged assets, headless
verification, no enabling incomplete gameplay. Step 1's complete acceptance
criteria remain unchanged.

Review focus: source assembly accumulation order, jaw angle/translation rates,
Ninja rotation lookup, initialization-time eye matrices and repeated updates.

## Task 1: Selected mode-1 facial deformation

Files: src/face_deformation.hpp/.cpp, src/face_math.hpp, src/face_model.hpp/.cpp,
tests/face_fixtures.hpp, tests/face_deformation_tests.cpp,
tests/face_model_tests.cpp, CMakeLists.txt, tools/room_inspect.cpp,
docs/actor-rendering-status.md.

Interface: decode_face_deformation(mask, prepared) -> FaceDeformationData;
deform_face(prepared, data, parameters) updates private model bytes atomically.

- [x] Share existing synthetic model fixture; add new deformation fixture.
      Observe explicit unimplemented decoding failure in face-deformation-tests.
- [x] Decode lists and their connection/normal references, jaw and tongue
      rates; validate signed counts, model vertices, extents and parameters.
- [x] Reset jaw/list vertices from immutable sources. Apply normalized muscle
      connections in the assembly's reverse accumulation order.
- [x] Apply source-aligned jaw rotation/translation and per-vertex rates;
      preserve unscaled translation for fractional jaw rates.
- [x] Apply tongue XYZ deformation from its own source vertices.
- [x] Capture original eye matrices during initialization, including scale;
      implement original eye-parent pitch/yaw updates.
- [x] Verify repeated updates, immutable sources, placement, malformed data
      and transactional failures in headless tests.
- [x] Run retail all-shape deformation/render probe, full tests, Windows
      package build and packaged headless smoke.
- [x] Request source-fidelity review; fix substantive findings, commit and
      record evidence. Packed lips and MASK scheduler still follow this task.

Verification: face-deformation-tests RED (decoder missing) -> GREEN;
signed eye-storage regression RED -> GREEN. Read-only source review found
one allocation-before-bounds defect: constrained-allocation regression RED
(std::bad_alloc) -> GREEN with aggregate validation before native allocation.
Source muscle/jaw/tongue/eye math and order had no other substantive findings.
Final build: local/facial-deformation-verified-build.log,22/22CTest passed,
Windows executable and ZIP rebuilt. Full retail probe:
local/facial-deformation-final-map-runtime-check.tsv,205rooms/960runtime ticks,
93shape samples,88frames with changed triangles; selected parents tooth7,
tongue11,eyes9/13 match the pending ControlMask runtime copies.
Fresh packaged smoke: local/facial-deformation-runtime-ui-1,241ticks/240frames,
53movement ticks,10focus-blocked ticks,menu return and INI verified.
The INI retry correction is separately committed as39fd4cc. No original asset
bytes or normal profile changes are tracked. Step1 remains active/incomplete;
next required work includes ControlMask's specific runtime joint-angle copies,
packed lips, enemy control/combat, audio and cross-room saves.
