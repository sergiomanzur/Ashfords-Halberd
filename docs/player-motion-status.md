# Native player asset dependency — 2026-10-02

The native core now reads the original player package's model/skin records,
100 motion slots (including absent entries), auxiliary data and per-model
texture blocks. High-bit texture records retain their 32-byte package-relative
alignment. Model and texture bytes remain owned data pending actor rendering.

MTN decoding follows `binfunc.c::bhMnbBinRealize`. Dense translation floats,
packed 16-bit angle keys and optional frame attributes are decoded explicitly.
Sampling follows `Motion.c` at original 16.16 frame positions, including
`SetMtnSlow` signed-angle interpolation and truncation toward zero. Native
frame advancement preserves `bhSetMotion`'s post-sampling order, single loop
adjustment, fractional frames, boundary return codes and complemented
ping-pong direction. Arithmetic explicitly retains signed 32-bit wrap behavior
without C++ signed-overflow assumptions. The corridor pose probe uses this
counter through all 150 frames and verifies its final wrap. Transition
blending, mirroring, actor-controller integration and player-specific joint
mapping remain pending; this counter alone is not runtime actor playback.

Skin records follow `njplus.c`: object/group associations, per-vertex joint
indices, ten-step rotation weights, and the preserved auxiliary low word.
Skin deformation and textured actor rendering now use these bindings;
see [actor rendering status](actor-rendering-status.md).
The default skin algorithm blends a joint's unrotated and rotated transforms;
the alternate FM mapping still needs separate implementation.

The package decoder caps bytes, record counts and cumulative decoded keys.
This prevents shared key offsets inflating many small motion records into
unbounded allocations. Synthetic tests cover malformed extents, non-finite
keys, absent slots, skin indices/weights, ordinary/aligned texture association,
fractional angle wraparound and aggregate budgets. Deterministic mutations also
passed with GCC standard-library assertions enabled.

Headless `AshfordPlayerInspect SYSTEM.AFS 10` decoded Claire's default package:
8 model records, 100 motion slots, 60 present motions, 1,810 total frames,
and 738 main-model skin vertices across 22 joints. Every frame and half-frame
was sampled. Entries 11–14 also decoded and sampled successfully. Entries
15–17 are 11-byte unavailable-asset records, not complete player packages.
No retail data is included in source, tests or distribution.

The native room-motion-bank decoder follows `room.c::bhSetRoomMtn`:
length-prefixed MTN records with empty index slots and a `0xffffffff`
terminator. It resolves the bank after the per-enemy pointers in RDX section
24, bounds records by the next motion/auxiliary or outer-header section, caps
record count and aggregate keys, and supplies the original 512 runtime slots.
This bank is distinct from regular enemy motions: CommonCtr motion selection
uses the room bank. The headless retail room probe decoded 624 motions across
205 rooms and sampled 1,872 first/middle/last poses, including the corridor's
motion 0. This verifies decoding and sampling, not actor-controller playback.

Step 1 remains incomplete. Next is application integration and original
movement/collision, room interactions
and transitions, enemy combat, inventory, audio and versioned save/load.
