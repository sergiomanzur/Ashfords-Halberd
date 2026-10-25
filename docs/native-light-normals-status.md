# Authored and animated normals for original native lighting

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

Chunk51 now decodes both xyz records. Authored normals at record+16 reach each
emitted corner using the current linear model matrix in float arithmetic.
Translation, inverse-transpose correction and normalization are not applied:
Ninja retains the original vector length. Strip64/65/66 winding, cache offsets
and subsequent vertex chunks retain the associated normals. Nonfinite consumed
xyz or transformed results reject; unused position/normal W lanes are not xyz.

Ordinary and facial skin now pose the normal alongside the position, on private
model bytes. The source's ten literal float weights replace multiplication by
0.1; level9 directly selects the rotated output. Bind conversion still changes
positions only. The existing owner identity drawing matrix prevents another
transform. Facial joint remapping and skipped eye objects preserve their
respective skinned and rigid paths. Morphed normals reach posed actor meshes
and room composition, while retained input model/skin data stays immutable.

Numerical fixtures capture32 matrices/vectors from the unchanged normal
equations in MIT recvx-decomp ps2_NinjaCnk.c::njCnkCvVn, compiled separately
as native scalar code. Ten level bit patterns come from njplus.c's literal
table. Every normal equation fixture matches output float bits. This evidence
does not execute PS2 VU code, validate all existing matrix/trig construction,
or substitute for independent original-game rendering/behavior acceptance.
See tests/normal_reference_fixture.hpp and
local/normals-source-reference-capture.log.

Behavioral BASE checks rejected the old implementation: missing emitted
normals, accepted NaN xyz and incorrect literal weight bits. After the decoder
alone, skin/morph/levels remained failing; the final skin implementation passes
all eight focused checks, including explicit FM remap/skipped eyes and linked
cached placement. Evidence: local/normals-baseline-red.log,
local/normals-decoder-stage.log, local/normals-skin-red.log and
local/normals-skin-green.log. The complete Windows build, including expanded
four-vertex winding and second-chunk cache coverage, passes123/123 and installs
and packages successfully: local/normals-full-build.log. A fresh packaged dummy
video/audio/software profile passes241ticks/240renders/53movement and10 simulated
focus-blocked ticks, generates192800 audio frames, submits184000 and exits
with queue0/Effects100: local/normals-final-packaged-smoke.log. The exact
six-file ZIP and packaged/installed EXE SHA256 comparison pass:
local/normals-final-package-integrity.log. Source formatting and diff checks
also pass. One fresh read-only review found no Critical/Important defect and
one Minor coverage improvement: strengthen level9's direct-selection fixture
with finite opposing endpoints whose difference overflows. Production already
selects the rotated output directly. This test improvement remains deferred
and tracked for subsequent lighting/original rendering acceptance. No second
review or additional fix pass was performed.

The production retail audit loads the actual corridor without script edits,
advances120ticks, and observes13098 room corner normals and4194 player corner
normals, all finite/nonzero. Player normal hashes change on120 frames and
match player-only room composition on every tick. The separately decoded
cemetery geometry has17385 finite/nonzero normals; this is authored geometry
inspection, not successful cemetery gameplay entry.
Evidence: local/normals-retail-audit.log.

Original command53 still requires full light selection, linked/dynamic timing
and vertex color/pixel consumption. No light instruction is accepted as a
flag-only substitute. The original camera/controller, remaining gameplay,
audio/effects, cross-room saves and independent reference acceptance remain
required by the complete Step1 gate. New Game remains disabled.
