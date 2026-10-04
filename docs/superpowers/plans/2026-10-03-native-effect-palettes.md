# Native effect palette textures implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline. Follow
> superpowers:test-driven-development and verification-before-completion.

**Goal:** Decode the cemetery's actual shared-image effect408 palettes into
distinct native textures, as required by the original spark animation.

**Architecture:** Keep the existing decode_texture_pack/decode_tim2 interfaces.
Move texture parsing from scene.cpp into a focused texture.cpp. Read PLI
metadata and emit one texture for each supported original palette variant;
apply the original palette selection and alpha preparation on owned pixels.

**Tech stack:** C++17, CMake/Ninja, GCC, Python source capture, CTest.

**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
BASE:8f6e170c7ac244f8e00a20ada6cc02560fdb7486.

## Global constraints

- Native Windows code; use the user's imported USA assets.
- All checks headless; fresh ignored profiles and SDL dummy drivers.
- No proprietary assets in Git or distributions; pinned upstream unchanged.
- Keep the complete two-room gameplay, sound, combat, inventory and save gate.
- Opcode173 stays unsupported until its original controller and drawing consume
  the type change. This prerequisite does not complete the spark or Step1.

## Source and design

Original ps2_texture.c::bhSetMemPvpTexture retains PLI metadata until replaced.
Its isVQ returns zero for PictFormat5..8, choosing shared palettes. The inspected
effect408 has format5, indexed4-bit pixels,256x256, two automatic-bank descriptors
(TEXflag0,bank255,offset0),128-byte picture header and256 palette bytes. The
source advances32 color entries per16-color palette; ClutCopy selects0..7 and
16..23. Ps2CheckTextureAlpha clears colored black transparency, expands original
ARGB4444 alpha, then halves it before the GS draw. OrgColorType6 halves alpha;
types0/1 only clear opaque black. Preserve existing direct atlas decoding.

Tim2_Format_Check converts the file's linear indices for PSMCT32 upload. Native
decoding uses those original indices directly; accepting format5 does not mean
interpreting a converted GPU transfer buffer. Reject already-prepared headers,
other shared formats, palette updates/shared explicit banks and unsupported
original color kinds until their actual services are implemented.

## Review focus

1. PLI metadata spans multiple images and zero-size PLI records: preserve it.
2. Variant order and padded16-color CLUT addressing: animation selects the
   correct palette rather than a duplicated image.
3. Original black and low-alpha handling: match scalar source captures.
4. Truncated metadata, variant extents and excessive pixel counts: reject
   before out-of-range reads or large allocations.
5. Legacy room/player texture behavior: keep non-PLI decoding unchanged.

## Task1: decode genuine palette variants

Files: create src/texture.cpp,tests/effect_texture_tests.cpp,
tests/effect_alpha_reference_fixture.hpp,docs/native-effect-textures-status.md;
modify src/scene.cpp,CMakeLists.txt. Private capture/evidence stays in local/.

Interfaces consumed/retained:
```cpp
Texture decode_tim2(const Bytes &data, size_t offset, size_t size);
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset);
```

- [x] Capture80 alpha cases from unchanged source helper bodies in a C harness,
  including black and nonblack inputs and original color types0,1,2,6.
  Write synthetic two-palette indexed textures; assert distinct literal RGB,
  nibble order, padded CLUT indices8/15, alpha, and persistent PLI metadata.
  RED command: compile tests against current libashford_assets and run variants.
  Expected: Unsupported texture pack record, demonstrating the actual missing
  decoder rather than a missing declaration.

  The key failing assertion is:
  ```cpp
  auto textures = decode_texture_pack(shared_palette_fixture(), 0);
  require(textures.size() == 2);
  require(textures[0].rgba[0] == 17 && textures[1].rgba[0] == 68);
  ```
- [x] Move the legacy decoder, implement bounded PLI metadata and native
  shared palettes, including scalar alpha transformations. Add variants,
  source-reference, malformed-input CTest modes. Format new files; run those
  modes and existing scene/player/actor tests.
  Expected: new modes pass; source-reference reproduces all80 captured cases.
- [x] Run full tools/build.ps1, packaged dummy SDL smoke with a fresh UUID
  profile, ZIP/EXE integrity check. Rerun the unchanged retail texture audit:
  expected effect408 produces2 actual256x256 textures in source order;
  the inspected ISO stores identical palette bytes, so their decoded pixels
  must agree. Independently authored fixtures prove distinct palettes differ.
  all required shared effect packs decode or expose an explicit source boundary.
  Run unchanged entry audit: expected corridor120 ticks, cemetery15/173at4,
  revisit139at276. Scripts, flags and controllers are not bypassed.
- [x] One fresh read-only review for this increment under requesting-code-review.
  Rule on findings; fix Important/Critical with RED/GREEN in one author pass;
  defer Minors. Record evidence, verify staged paths/diff, commit locally.

Final review:0 Critical,0 Important,0 Minor. No correction pass required.
All recorded execution handles finished; no original-game acceptance claim.

The previous goal turn was a status answer and made no implementation progress.
Current state revalidated clean at BASE; the private genuine retail audit changed
the next action from controller integration to its missing palette decoder.
Existing continuous implementation authorization covers this prerequisite and
inline execution without another routine design/approval handoff.
