# Original native room lighting

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

Room light records now decode into owned native work with explicit little-endian
fields, bounded tables and no disk-pointer casts. Normal-room initialization,
cut hide masks, reserved normal slots, event tables, first directional/three
point selection and original screen margins feed the native controller. The
source's dynamic types0..13/100/101 retain counters, shared RNG and cached
parameters; player/enemy/object/item/effect links use current model/joint caches.
Inactive targets clear the light flags before consuming stale model owners.
Invalid control cannot publish partial work, RNG or scenario flags.

Authored normals now reach the original native vertex-color pipeline. It
preserves directional/point branches, Easy/Simple single/multi tables, material
diffuse/ambient/specular/alpha and the original literal17 specular operation.
The actual VU point branch includes far equality and executes its delay-slot
diffuse at/inside near; this differs from the unused scalar fallback. Retained
material commands17..23, four-byte exponent command3 and size-counted command16
remain aligned, including commands after the last strip or without geometry.

Native float attributes pass through clipping before byte conversion, with
screen-linear color/fog and perspective-correct UV. Flat strips use the final
submitted vertex. Texture modulation divides by128; fog divides by256 even
when the source submits disabled-fog F255. Source material alpha registers
drive blending. Unsupported alpha combinations reject explicitly.

Original room drawing order now supplies per-mesh full/half lighting, the ST20
exceptions, unconditional main-actor half lighting, GM80 Easy drawing/flash
timing and item highlights that change only multi-light channels. Composition
copies work and publishes ending draw counters only after a valid rendered
frame. Runtime light clocks first advance at the original fixed-tick camera
boundary, not during room initialization.

Original four-byte command53 changes actual composed native pixels. Bound table,
index and extent checks precede mutation. Retail rooms always use the native
light state; synthetic pre-existing raster fixtures retain their own legacy
path. Unsupported dynamic-camera control still rejects rather than substituting
a static snapshot.

Independent source-derived numerical capture includes1344 dynamic-controller
samples and72 actual VU1 vertex-color samples matching native float bits. VU0
functions5/10 also match their VU1 counterparts in16 source capture cases.
The unchanged original controller switch and separately translated VSM expressions
compile in private harnesses; source hashes are recorded in
local/light-source-control-capture.log and local/light-source-vsm-color-capture.log.
These are native scalar/source evidence, not execution on original hardware or
independent original-game visual/behavior acceptance. Orbit8/9 additionally have
opposite quarter-circle behavior tests. The normal level9 fixture now uses
finite opposing endpoints whose intermediate difference would overflow; the
existing direct endpoint implementation passes without changing production
normal code.

The corrected Windows build passes140/140 CTest cases, installs and packages:
local/lighting-reviewed-edge-full-suite.log. The fresh packaged dummy SDL profile passes
241ticks/240renders/53movement and10 focus-blocked ticks, generates192800 audio
frames, submits184000 and exits with queue0/Effects100:
local/lighting-reviewed-edge-packaged-smoke.log. The exact six-file ZIP and packaged/
installed EXE SHA256 comparison pass: local/lighting-reviewed-edge-package-integrity.log.

Verification evidence is retained in local/native-room-lighting-progress.md:
BASE failures, targeted RED-to-GREEN material/selection/link/entry-clock cases,
analytic clipped RGB/fog and foreground depth, drawing policy, complete CTest,
packaged dummy SDL runtime and exact ZIP/EXE comparison. One final fresh
read-only review completed with4 Important corrections and1 deferred Minor.
The four corrections pass17 focused cases and the full140-case suite.
All checks are headless and use ignored isolated
profiles.

The corrections use the actual VU1/VU0 constants for color function5 and the
original function10 accumulator seed. Source Single rejection uses all three
camera-space normals and retains preceding material changes. Deferred Double
transparencies capture their submission attributes, preserve full strip depth
averages and stable4096-bucket ordering, and use source GEQUAL/alpha128 depth
rules, including framebuffer-only zero-alpha blending. Shared strip edges blend
once for either winding; the half-open coverage convention is corroborated by
the [primary GS rasterizer reference](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/SW/GSRasterizer.cpp#L988).
This is algorithm reference evidence, not original-game execution or acceptance.
Light control precedes
events; current drawing uses a work snapshot while next-tick event53 enable
changes survive successful draw publication.

The review's Minor retained GM80 Easy multi-light/highlight state difference is
deferred. Exact original-game acceptance and PS2 numerical precision remain
unproven; the VU0 deferred function7 retained-vector context also needs further
source binding. The existing geometric near plane1 also differs from the
source game setup2; source depth mapping uses2/20000. Resolve this clipping
boundary in the original camera/render integration before slice acceptance.
These limits remain part of the full native slice work.

The corrected unchanged retail entry probe enters both the corridor and cemetery without
script edits or forced flags. Command53 no longer blocks cemetery entry. Its
first main tick still reaches original instruction13, bhDieCk, at offset6;
the cemetery revisit still requires instruction139 at276. A separate read-only
light-module audit uses the actual authored lights and pixels, while corridor
production gameplay advances120 ticks (local/lighting-reviewed-edge-entry-audit.log
and local/lighting-reviewed-edge-retail-audit.log). The cemetery module audit does not
claim its complete main loop runs.

Full Step1 is still unfinished: the connected-room gameplay, original enemy
orchestration/combat and healthy start state, inventory/interactions, complete
decoded sound/effects, original camera control, versioned cross-room save/load
and independent original-game acceptance remain required. New Game stays
disabled. The optional modern over-the-shoulder camera belongs to Step2.
