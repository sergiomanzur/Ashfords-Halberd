# Native original room fog component

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

ROM_WORK fog color and signed near/far values now decode separately from both
authored camera variants. Source command76 consumes six bytes: opcode, alpha,
red, green, blue and mode. Mode0 stores ARGB and requests an override; any other
mode retains that color, clears the override flag and requests a refresh.
Truncated input rejects before changing native work or advancing its cursor.

The native table generator and inverse-Z lookup preserve ps2_NaFog.c's 128
entries, quantized density, float operations and interpolation. Positive near
values such as128 are intentional source data. Thirteen complete table hashes,
density bits and182 sampled float results match fixtures captured from the
unchanged MIT source functions compiled as native code. This numerical reference
does not replace an independent comparison against original gameplay/rendering.
Evidence: local/fog-source-reference-capture.log and
tests/fog_reference_fixture.hpp.

Source EOR fog control retains override masking, enable/disable state and dirty
flags, the distinct cut/eye/keyframe countdown branches and force-disable behavior.
Room entry selects destination parameters before initialization scripts. Actual
cut changes and script overrides reach current scenario work and composed scenes;
failed table generation preserves the prior work, flags and cached selection.
Eye fog uses the original ROM fallback and minus4 delay. The full eye/keyframe
camera geometry and controller binding still require implementation. Original
normal camera flags40/80 adjust fog ranges from the controlled camera every
frame. Those bindings now reject transactionally, including already cached
cuts, until the actual dynamic camera controller can provide their values.
Cemetery cuts2/3 contain these flags; static values do not satisfy their behavior.

Rasterization evaluates original fog at vertices, carries that attribute through
clipping, quantizes its vertex value and interpolates it in screen space before
mixing shaded color. The GS numerical convention uses F/256; corroboration:
[official PCSX2 hardware shader](https://github.com/PCSX2/pcsx2/blob/master/bin/resources/shaders/opengl/tfx_fs.glsl).
That source is research evidence only; the native runtime has no emulator
dependency. Normal/half objects and items suppress fog only under original
mdflg10/ST2 conditions; special objects retain fog. Their unconditional restore
can re-enable fog after EOR force-disable, including after clipped geometry.
Completed draw state persists into the next EOR/frame. Rejected frames publish
neither retained ambient material nor completed fog state.

The six focused fog checks pass, including actual mixed pixels, varying depth,
clipped geometry, transparent rejection, live command76 updates, draw grouping,
cross-frame restoration and failed cut recovery. The first full run was113/114:
a restoration fixture omitted normal-object mdflg10. Its pixel/state assertions
remain intact; the fixture now authors that source flag. With restoration
temporarily withdrawn the valid regression failed, then all six passed when the
original restore returned. Evidence: local/fog-restoration-valid-red.log and
local/fog-restoration-valid-green.log. The corrected full Windows build passed
114/114 checks and installed/packaged successfully:
local/fog-full-build-corrected.log. The ZIP has exactly six runtime/readme/license
files and its EXE matches the installed EXE by SHA256:
local/fog-final-package-integrity.log. A fresh packaged dummy runtime completed
241ticks/240renders,53 movement ticks and10 simulated focus-blocked ticks,
generated192800 audio frames, submitted184000 and returned with queue0.
Evidence: local/fog-final-packaged-smoke.log.

One fresh read-only review found one Important unsupported-dynamic-binding issue
and two Minor rendering/coverage issues. The single author fix pass observes
all nine dynamic mask/binding cases fail before the guard, then seven/seven fog
checks pass. Rejected binding preserves all fog work, flags, effects and cached
selection, while eye fog retains ROM precedence. Evidence:
local/fog-dynamic-binding-red.log and local/fog-dynamic-binding-green.log.
The post-fix full Windows build passed115/115 and installed/packaged:
local/fog-review-fix-full-build.log. A second fresh packaged dummy profile
again completed241ticks/240renders/53movement, with192800 generated audio frames,
184000 submitted and queue0: local/fog-review-fix-packaged-smoke.log. The final
six-file ZIP/EXE hash check and native formatting/diff checks pass:
local/fog-review-fix-package-integrity.log and
local/fog-review-fix-format-check.log. No second reviewer is used.
Deferred Minors: source disabled/suppressed F255/256 primitive mixing can
differ by one channel level from the current bypass; unequal fog at generated
clipping vertices and explicit fogged foreground/background rejection need
stronger source-derived pixel fixtures. Both remain tracked for original-rendering
acceptance; the full Step1 gate is not waived.

Unchanged retail source inspection finds corridor cut0 fog07060F/128/-110 and
cemetery cut0 fog151521/128/-90. Actual cemetery command at416 is
76,255,21,21,33,0. The native entry probe passes it and next requires light
command53 at422. The revisited cemetery still requires computer139 at276.
RDX1 enters and advances120 ticks. No opcodes are skipped and original scripts
remain unchanged. Evidence: local/room-fog-source-inspect.log and
local/fog-review-fix-entry-audit.log. Cemetery gameplay entry is not complete.

A separate read-only retail fog audit loads the actual corridor through the
production entry/tick/composition/rasterizer path: cut0, color07060F,
near128/far-110, original table match and76800 visible pixels. Its cemetery
inspection only decodes an authored view, matching the original table and
producing73948 visible pixels; it does not demonstrate gameplay entry. The next
actual light command is53,1,4,0. Evidence:
local/fog-review-fix-runtime-audit.log. The diagnostic uses native archive/decoder
interfaces without skipping scripts or modifying runtime flags.

Source direct/half lighting, dynamic camera/clipping selection, the remaining
effect rendering and independent original visual comparison remain required.
Complete Step1 still requires both connected original rooms, enemy gameplay,
combat, inventory, complete decoded sound/effects, versioned cross-room saves
and original reference acceptance. New Game remains disabled.
