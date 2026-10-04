# Native effect palette banks implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, with
> superpowers:test-driven-development and verification-before-completion.

**Goal:** Load the original palette-only records, retain the shared palette
memory across effect packs and resolve texture aliases against its current
colors, including the real two-room slice's family54.

**Architecture:** A native effect texture library owns the4096 original palette
words, source allocation cursor and texture bindings. Indexed images retain
their original indices; bindings select the current bank when resolved. Existing
decode_texture_pack uses a local library and materializes at pack completion;
the persistent interface permits the room loader to share banks across families.

**Tech stack:** C++17, CMake/Ninja, GCC, Python, CTest.

**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
BASE:2cd5d84 (resolve full SHA in the private task ledger).

## Global constraints

- Keep the complete two-room gameplay/combat/inventory/audio/save gate active.
- Native Windows code, user-owned USA assets, pinned upstream unchanged.
- All execution checks headless; SDL dummy drivers and fresh ignored profiles.
- No retail assets in Git/packages and no external publication.
- Opcode173 and New Game stay gated until their real consumers/full gate work.
- Preserve the existing ordinary room, player and actor texture API/semantics.

## Source and design

ps2_texture.c::bhSetMemPvpTexture retains PLI descriptors, allocates automatic
banks from sys->ef_pbnk (initial32), adds1 for4-bit or16 for8-bit and subtracts32
when the next bank reaches64. Each descriptor writes16/256 prepared colors at
bank*16+offset. ClutCopy and ClutCopy256 select the original padded/permuted
source colors. Every nonzero TEXflag updates colors without emitting a binding.

The emitted binding uses the low6 bank bits packed into gidx. Primary loads
reuse the source njLoadTexture identity (low26 global-index bits, bank); later
emissions in the same image alias the previously emitted image, even if an
earlier descriptor was palette-only. Keep this distinction from descriptor
upload addresses. Reject an alias without a preceding binding. Bank values
above63 address the original4096-word CPU palette storage but the normal draw
samples its first1024 words. Native8-bit sampling selects the aligned256-color
block (bank&0x30)*16;4-bit sampling selects bank*16.

Resolve after all uploads, rather than freezing each upload's colors. A room
will use the persistent library across all effect families. The current task
implements this resource service; binding that library to every effect
controller/draw queue remains mandatory subsequent integration.

## Review focus

1. Palette-only records consume an automatic bank and no texture slot.
2. Bank wrap/explicit overlap recolors previously loaded indexed images.
3.8-bit CLUT writes, padded4-bit selection and prepared alpha stay source-faithful.
4. Alias/primary identity retains the source image; it does not fabricate one.
5. Invalid extents, undefined sampled colors and aggregate pixel exhaustion
   reject explicitly; a failed pack leaves its incoming library unchanged.

## Task 1: persistent native palette memory

Files: create src/texture.hpp,tests/effect_palette_bank_tests.cpp,
tests/effect_palette_reference_fixture.hpp;
modify src/texture.cpp,tests/effect_texture_tests.cpp,CMakeLists.txt and
docs/native-effect-textures-status.md. Private probes/evidence remain local/.

Interfaces:
```cpp
struct NativeEffectTextureLibrary; // Palettes, cursor, bindings, shared images.
struct NativeEffectTextureRange { size_t first = 0, count = 0; };
NativeEffectTextureRange load_effect_texture_pack(
    const Bytes &, size_t, NativeEffectTextureLibrary &);
Texture resolve_effect_texture(const NativeEffectTextureLibrary &, size_t);
std::vector<Texture> decode_texture_pack(const Bytes &, size_t); // retained
```

- [x] Write a family54-shaped synthetic regression through the existing API:
  ```cpp
  record(b, PLI, metadata(2)); record(b, TIM2, picture(2));
  auto descriptors = metadata(2); word(descriptors, 16, 0x0000ff01);
  record(b, PLI, descriptors); record(b, TIM2, picture(2));
  record(b, PLI, metadata(1)); record(b, TIM2, picture(1)); terminal(b);
  require(decode_texture_pack(b, 0).size() == 4);
  ```
  Run the test compiled against BASE's core.
  Expected: genuine behavior RED, unsupported palette update.
- [x] Implement validated palette memory, automatic allocation and nonemitting
  uploads, native indexed image identity/aliases and current-color resolution.
  Stage each pack before committing its library. Count every emitted binding's
  pixels against64M before allocating output; reject writes beyond4096 colors.
  Keep shared formats6..8 and prepared headers guarded. Update obsolete negative
  descriptor fixtures to actual invalid extents and preserve legacy tests.
  Expected: new updates test GREEN and existing texture cases GREEN.
- [x] Add literal bank1/16 allocation,32..63 wrap,4/8-bit color addressing,
  explicit offsets/high-bank masking, cross-pack recoloring/immutable image,
  primary reuse/previous-image alias, update-only/first-alias errors, transactional
  malformed extents, undefined colors and budget cases. Capture unchanged
  ClutCopy/ClutCopy256 helpers privately and compare their ordering with fixtures.
  Run CTest focused palette/texture plus legacy scene/player/actor checks.
  Expected: all source scalar/literal and focused tests pass.
- [x] Run tools/build.ps1, fresh packaged dummy SDL smoke and ZIP/EXE integrity.
  Rerun the genuine source-cursor texture audit against the persistent library.
  Expected: RDX1/2/3 cursors5/10/9; every inspected effect family loads,
  family54 emits4 bindings and family408 emits2. Production entry boundaries
  remain unchanged (corridor120; cemetery15/173at4; revisit139at276).
- [x] One fresh read-only whole-increment review. Address Important/Critical
  findings in one RED/GREEN author pass; record rulings/minors and local evidence.
  Verify staged paths/whitespace and commit locally. Keep the full goal active.

The preceding status-answer turn made no implementation progress. BASE was
revalidated clean; genuine family54 records and original palette storage select
this next action. Continuous user authorization covers this source port and
inline execution without a repeated approval/handoff. Preserve private evidence
instead of deleting it after review.

Final review:0 Critical,1 Important,0 Minor. Mixed ordinary-backed alias primary
identity was verified against source, reproduced RED and corrected in one pass.
New identity/dimension/budget/no-false-bank0 regression passes; final9/9 focused
modes,152/152 full suite, fresh packaged dummy smoke/integrity and recompiled
real resource/production activation audits pass. No second review is needed.

Additional next-action evidence: pinned system.c loads SYSTEM.AFS entry1 before
bhInitEffect. Its four models0/1/7/13 precede aligned texture cursor8448. The
native library resolves40 global textures, so subsequent integration can bind
the actual global and room families rather than inventing a missing resource.
