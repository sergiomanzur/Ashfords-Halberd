# Native shared-palette effect texture prerequisite

Step1 remains incomplete. The real cemetery effect texture resources now load,
including their palette-only updates and shared banks. This does not enable
opcode173, simulate the emitter or draw its particles. New Game remains gated
until the full two-room slice passes.

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

Unsupported shared formats6..8, prepared headers, other original color kinds
and unsupported CLUT layouts reject explicitly. PLI lengths/counts, complete
palette/image extents and a64-million aggregate pixel budget are checked before
RGBA materialization. Each emitted binding, including image aliases, counts
toward that budget; palette-only uploads emit no binding or output pixels.

## Shared native palette memory

texture.hpp exposes NativeEffectTextureLibrary, load_effect_texture_pack and
resolve_effect_texture. One library retains the original4096 CPU palette words,
initialized-color map, automatic bank cursor and shared immutable indexed images.
Automatic banks start32, advance1/16 for4/8-bit images and subtract32 upon
reaching64. All nonzero TEXflag values upload colors without emitting a texture.
Explicit banks and offsets write the corresponding original palette addresses.

The normal draw samples the first1024 colors. A binding retains the low6 bank
bits;4-bit sampling uses bank*16,8-bit sampling uses (bank&0x30)*16. Explicit
high-bank uploads retain their CPU staging colors without inventing a change in
the lower draw bank. Writes outside4096 colors and samples of unloaded colors
reject. A failed pack preserves its incoming library, including prior colors,
allocation cursor, bindings and pixel budget.

Primary shared image identity follows njLoadTexture's low26 global-index bits
and bank. Ordinary images retain their full source global index and distinct
no-shared-bank identity. Nonfirst emitted descriptors copy the preceding image
and its identity, including an ordinary RGBA backing, even when an earlier
descriptor was palette-only; an alias without a preceding image rejects.
Resolving samples the current palette, so overlapping uploads/wrapped banks
recolor already loaded images. decode_texture_pack keeps its public API and
resolves a local library after completing the pack. The persistent interface
must still be bound to room effect work and original draw queues.

## Initial palette decoder evidence (commit2cd5d84)

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

## Persistent bank verification

- Genuine existing-API RED for the family54-shaped record sequence: unsupported
  palette update at BASE, then GREEN with four emitted textures.
- Four bank modes cover cross-pack recoloring, nonzero update flags, neighboring
  offsets, automatic wrap,8-bit upload/draw addressing, high CPU staging banks,
  primary identity reuse, aliases, malformed-record rollback and pixel budgets.
- New source reference captures unchanged ClutCopy/ClutCopy256 C helpers:
  all16 small selections and the256-color ordering hash agree. Existing80
  original alpha cases still pass. These establish scalar source behavior.
- Fresh final Windows build/install/ZIP:152/152 tests pass after the review fix
  in local/effect-palette-bank-final-full-suite.log. Packaged SDL dummy smoke has241 ticks,
  240 renders,53 movement ticks,10 focus-blocked ticks,192800 generated and
  184000 submitted audio frames. Exact six-file ZIP/EXE integrity passes.
- Private persistent-library audit verifies actor/effect cursors5/10/9 and
  resolves every inspected family after all its room's uploads: RDX1 has1
  family/1 texture, RDX2 has14 families/18 textures, RDX3 has11 families/12
  textures. Family54 emits4 actual256x256 textures;408 emits2 with the retained
  hash4904945646251938359 and6443 nonzero-alpha pixels each. No family is skipped.
- Fresh production startup/work audit verifies10 roots,10 bodies,52 auxiliary
  actors and120 corridor ticks. Post-fix activation audit: cemetery15 ticks
  then173at4, revisit139at276/computer0. No opcode/controller is bypassed.
- One fresh bank review:0 Critical,1 Important,0 Minor. The mixed ordinary/
  shared alias identity case reproduced a genuine behavioral RED. Retaining
  identity with either backing passes its new three-load regression, actual
  reused image dimensions/emission charge and no-false-bank0 assertion. All9
  focused modes and the152-test whole suite pass after that single fix pass.
- Additional source/resource inspection locates global effects in SYSTEM.AFS
  entry1. Its four original model chunks (IDs0,1,7,13) precede the aligned
  texture pack at8448. The native service resolves40 textures/2662912 pixels,
  leaving automatic cursor32 with384 initialized colors. This identifies the
  actual base resources for the next binding job, not completed global drawing.

Private bank evidence uses local/effect-palette-bank- logs; final-full-suite,
final-packaged-smoke,final-package-integrity,final-retail-audit and
post-fix-entry-audit are the post-correction results. identity-red/identity-green
record the review fix. Global resource evidence is in
local/global-effect-resource-audit.log and global-effect-native-resource-audit.log.
Source capture SHA256 for ps2_texture.c is
5995b32e032ee903d24379e407d00c95489ed0c1bc7d868496f8c11c039defad.

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

The private live-work inventory confirms that corridor effect92 needs its
original bhEffFil screen-filter controller/drawing. Cemetery entry has active
work IDs32,36,39,52,54,59,70,71,72,77,92,100,101,102,103,116,154,158,164,165,179
plus existing shadows and resource-only IDs400/404/408. Source bhJumpEffect
maps the listed IDs30..84 to bhEffDmy, whose real behavior clears flags; these
are texture-family records, not separate particle controllers. IDs>=400 likewise
release their work in bhControlEffect. They must free their real slots during
the ascending sweep. Paused103/181/201/218 work must behave correctly when
enabled. Porting179/180 alone does not establish a complete effect pass.
