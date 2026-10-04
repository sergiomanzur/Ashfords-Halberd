# Authored normals required by original native lighting

Spec: `docs/superpowers/specs/2026-10-02-windows-port-design.md`.
BASE: cd59e00. Execute inline under the user's continuing Step1 authorization.
The complete Step1 goal retains both actual connected rooms, original cameras,
player/enemy behavior, combat/inventory, complete audio/effects, cross-room saves
and independent original reference acceptance. This prerequisite does not enable
New Game or satisfy the original light controller/command53 pixel consumer.

Source: ps2_NinjaCnk.c::njCnkCvVn, vsm/ps2_vu0.vsm::CALCPOINT,
njplus.c::npCalcSkin/npCalcSkinFM and ps2_NaMatrix.c::njCalcVector.
The actual chunk51 normal is xyz at vertex record+16. Transform it with the
linear current matrix, without translation, inverse transpose or normalization.
Skin blends the unrotated and rotated vectors with the source's ten literal
float levels; level9 selects the rotated vector directly. FM skips objects11..14
and remaps source joints greater than10 by four. Morphing precedes skinning.

## Interfaces and constraints

Append `Vec3 normal{}` after Corner::u/v so existing positional aggregate
initialization keeps its meaning. Scene decoding populates it for every actual
chunk51 vertex and copies it with strip winding into every emitted corner.
Add `detail::ninja_vector(const NinjaMatrix&, Vec3)` for the float, affine linear
operation with checked inputs/results. Actor skin stores output xyz normals in
the private posed byte copy; the owner's identity drawing matrix prevents a
second transform. Keep original imported model/skin/morph data immutable.

Files: src/scene.hpp/.cpp, src/ninja_matrix.hpp, src/actor_render.cpp,
src/actor.cpp, tests/native_normal_tests.cpp, optional numerical fixture header,
CMakeLists.txt and docs/native-light-normals-status.md. No UI or settings change.
Headless only, isolated ignored profiles; no process focus, browser, audible
output, ISO picker, normal user profile, push/merge or asset redistribution.

## Task1: source-normal decoding and validation

- [x] Author a native-normal-tests target before production changes. Against BASE
  check nonfinite authored normals currently escape validation, and finite
  authored vectors fail to reach corners (compile-time detection permits a
  behavioral assertion against BASE's old Corner). Run each and retain RED logs.
- [x] Implement Corner::normal and shared float linear vector transformation.
  Decode record+16 with the existing endian/finite reader; preserve strip64/65/66,
  cache offsets and negative/alternating winding. Verify nonunit and zero normals,
  translated/rotated/scaled/sheared matrices and normal-specific overflow.
- [x] Compare direction arithmetic to fixtures captured from the unchanged
  njCnkCvVn scalar normal equations, compiled independently without SDK/emulation.
  Expected: every fixture matches float output bits. This is numerical evidence,
  not PS2 hardware or original gameplay acceptance.

## Task2: posed skin, facial and morphed normals

- [x] Against the new decoder but prior actor skin implementation, assert
  all ten skin levels reproduce original normal blending, including direct
  level9, nonunit vectors, nonuniform/sheared cached matrices and placement.
  Expected: behavioral RED due to unchanged bind normals or wrong level bits.
- [x] Use the source literal float level table in decode_skin. Write blended
  normals into the private output records and preserve source input bytes.
  Test ordinary and preconverted skin equivalence, FM skipped eyes/remapped
  joints, selected ranges, linked placements and rigid actor matrices.
- [x] Exercise existing morph interpolation followed by actual cached actor
  posing and room composition. Expected: changed vertices' morphed normals
  are skinned once, unchanged vertices retain source normals; rejected input
  leaves source assets, supplied cache and runtime scene unchanged.

## Task3: retail integration, package and final review

- [x] Run focused native-normal-tests, then tools/build.ps1 with redirected
  output. Expected: complete CTest suite, install and six-file ZIP success.
- [x] Read actual corridor via production load/tick/composition paths and
  verify finite normals survive moving player poses and scene composition.
  Inspect cemetery geometry without bypassing entry scripts. Keep unchanged
  entry probe proving command53/422 and computer139/276 still require consumers.
- [x] Run fresh packaged native_runtime_smoke.py dummy video/audio/software,
  exact ZIP/EXE hash and formatting/diff checks. All must pass before claims.
- [x] One fresh read-only gpt-6-astra/high reviewer for cd59e00..final diff.
  Re-grade findings; one author Critical/Important RED-to-GREEN pass and full
  suite, defer Minors and ledger each declined judgment. Local commit, goal active.

## Review Focus

1. Disk cache indices, strip winding, zero/nonunit normals and malformed consumed
   xyz/W lanes; a geometric fallback must not replace authored normals.
2. Float linear arithmetic, translation exclusion and finite overflow; no generic
   inverse-transpose or normalization changes original lighting inputs.
3. All exact source skin levels, FM remap/eye skips, matrix overrides, selected
   ranges, direct level9 and immutable input ownership.
4. Real morph-to-skin-to-room composition, linked/cached matrices and failures
   that must not publish partial output.
5. Retail/runtime/package evidence limits: preserved normals are a lighting
   prerequisite, not command53 support, visual parity or complete playable Step1.
