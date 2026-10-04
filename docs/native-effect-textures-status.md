# Native shared-palette effect texture prerequisite

Step1 remains incomplete. This change makes the real cemetery spark texture
resource decodable; it does not enable opcode173, simulate the emitter or draw
its particles. New Game remains gated until the full two-room slice passes.

## Implemented source behavior

The unchanged decode_texture_pack API now reads PLI records, retains their
metadata across images/empty PLI records and creates native RGBA variants from
the supported shared-image format5. Native texture parsing lives in texture.cpp;
scene.cpp continues to consume the same public reader. Ordinary room, actor and
player texture semantics remain under the existing interfaces and checks.

Shared indexed4-bit palettes use128-byte padded CLUT strides; indexed8-bit
palettes use1024-byte strides. ClutCopy/ClutCopy256's8/16 selection follows the
source. Independent automatic banks (TEXflag0,bank255,offset0) become distinct
textures, preserving source ani_ct ordering. Source original color types0,1,2,6
apply their black/ARGB4444/alpha preparation before native full-range RGBA export.

The shared reader requires an extended128-byte picture header and unprepared
source data. The source's Tim2_Format_Check converts linear file indices into a
PSMCT32 upload; the native decoder reads those linear file indices directly.
The original isVQ returns0 for formats5..8. Its name does not mean the format5
file already contains a converted GPU upload buffer.

Unsupported shared formats6..8, prepared headers, explicit/shared banks,
palette-only updates, other original color kinds and unsupported CLUT layouts
reject explicitly. PLI lengths/counts, complete palette/image extents and a
64-million aggregate pixel budget are checked before RGBA allocation. Each
supported palette emitted by a shared image counts toward that budget.

## Evidence

- Genuine baseline RED: variants reject Unsupported texture pack record in
  local/effect-texture-red.log. The earlier empty capture header compile error
  was corrected and was not counted as a failing behavior test.
- 80 captured alpha cases from unchanged ps2_dummy.c helper bodies, compiled
  as native C with GCC-O0. Captured output is checked for all80 cases. This
  establishes scalar source behavior, not PS2 or original-game acceptance.
- Synthetic checks cover two variants,4-bit nibble order/padded CLUT addressing,
  persistent/empty/replaced PLI metadata,8-bit CLUT strides/permutation, source
  black/alpha cases and malformed/unsupported metadata/images.
- Private retail inspection found effect408's two automatic palettes on one
  actual256x256 indexed4-bit format5 image. Its authored179 emitters have zero
  intervals and scales2,2 and4. Those values must feed the original one-shot
  controller rather than a fabricated periodic particle system.

- Final whole suite:146/146 in local/effect-texture-budget-full-suite.log;
  Windows build/install/package in local/effect-texture-final-full-suite.log.
  Aggregate-budget coverage rejects five valid16M-pixel variants before their
  320MiB of output allocation. Logical16-color headers with128-byte padding
  have an independently observed RED and a promoted GREEN regression.
- Fresh packaged dummy smoke:241 ticks,240 rendered frames,53 movement ticks,
  10 focus-blocked ticks,192800 generated/184000 submitted audio frames,
  queue0 and Effects100. Exact six-file ZIP and installed/ZIP EXE SHA256 agree.
- Fresh real-asset audit verifies actor/effect texture cursors5/10/9 for
  RDX1/2/3. Effect408 emits two256x256 textures; each has6443 nonzero-alpha
  pixels and hash4904945646251938359. Raw palette byte differences are zero,
  explaining their equal pixels. Distinct synthetic palettes produce different
  variants. Family54 exposes its original palette-only update as unsupported;
  all other inspected room effect-family packs decode.
- Fresh production entry audit: corridor120 ticks; cemetery15 ticks then
  opcode173 at4; revisit opcode139 at276. No scripts/controllers were skipped.

Private logs and imported images stay outside Git and the package.

One fresh read-only review inspected the entire prerequisite against BASE and
pinned source:0 Critical,0 Important,0 Minor. No corrective pass was requested.
Retained evidence was reviewed; the reviewer did not rerun execution checks.

## Remaining acceptance

Bind the real room texture families to runtime effect work, then port179/180
control, first-free births and ascending pool scheduling, their10-frame UV
animations, original draw queues, billboarding, source colors/blending/fog and
depth policy. Enable173 only after its actual native consumer works. Global
effect resources, other required controllers, complete sound/gameplay/inventory,
two-room traversal, versioned saves and independent original-game comparison
remain required for the full Step1 gate. Exact original texture filtering and
GS alpha/color quantization also require the drawing implementation and reference
acceptance; full-range RGBA export alone does not prove those properties.

The subsequent private live-work inventory also confirms that corridor effect92
needs its original controller/drawing. Cemetery entry has active controllers
32,36,39,52,54,59,70,71,72,77,92,100,101,102,103,116,154,158,164,165,179 plus
existing shadows and resource-only IDs400/404/408. The latter must be released
by the original id>=400 control branch. Paused103/181/201/218 work remains in
the pool and must behave correctly when enabled. Porting179/180 alone cannot
establish a complete effect pass or the full native playable slice.
