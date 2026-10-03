# Native animated actor rendering — 2026-10-02

The core now builds player joint matrices using the reference `bhCalcTree`
translation/ZYX rotation path. It adjusts skin vertices for bind translations,
blends the unrotated/rotated joint transforms using the reference ten-step
weights, applies actor position/yaw, and bypasses the skin-owner drawing matrix.
Rigid meshes use their posed joint matrices. Actor object indices are kept
outside the room camera's hide-mask range.

This follows the default `npSkinConvert`/`npCalcSkin` path. FM remapping,
motion-transition blending/mirroring, facial variants, attachments, lighting
normals and original render-state fidelity remain pending. The current byte-copy
deformation path explicitly rejects overlapping skin vertex buffers, including
sharing with a rigid mesh, rather than silently overwriting another pose.

Player texture packs now decode through the existing TIM2 path. Their four-byte
end marker is checked before reading a full record header; room texture packs
remain supported. Mixed textured and untextured strips preserve the `0xffff`
material sentinel. A review regression failed before the sentinel fix and
passed afterward.

Synthetic actor tests cover full/partial skin weights, animated translation,
combined-axis rotations with a rotated parent, position/yaw, two independent
skin groups, texture sentinels, cyclic/unreachable skeletons, bad vertex ranges,
non-finite inputs and shared/overlapping skin-buffer rejection. Deterministic
model mutations run through the pose path. Ten CTest suites pass headlessly;
the actor suite also passes with GCC standard-library assertions enabled.

The native diagnostic tool posed 858 snapshots across retail player packages
10–14. Claire's primary model and textures were also rendered inside retail
room 0 using the room's camera, visibility masks and shared depth buffer.
Saved images were inspected directly as files; no application window opened.
The diagnostic placement is the first camera area's midpoint, not the original
player spawn. Retail bytes and previews remain excluded from Git/distributions.

`AshfordPlayerInspect SYSTEM.AFS 10 --pose-check` validates poses without writing
images. An optional fresh output directory saves previews; appending
`RDX_LNK.AFS room-index` also saves combined room/actor camera previews.

Room enemy model banks now decode natively from RDX section 20, preserving
active-enemy pointer ordering, SKIN association and MASK bytes. Enemy texture
packs are assigned from the separate section-32 table using the reference
linear cache address (enemy ID, model version and textured-model ordinal); models without
their own texture list inherit the preceding list. Bounds cover records,
pointer tables, section extents and aggregate copied asset bytes. Tests cover
inactive authored slots, multiple models, inherited lists and cache separation
by ID/version. Retail graveyard model version 32 aliases a later ID's cache
slot, as in PS2 array-address arithmetic; bounds apply to the entire cache
allocation. An alias regression verifies shared textures without consuming
another pack. The headless room probe posed the retail corridor actor for all
150 frames of room motion 0 with its original textures. This verifies asset
loading and native skinning. Native event-driven corridor playback now passes;
application gameplay integration remains pending.

Whole-actor rotation now supports all three signed angle components using
`njRotateXYZ`'s Z–Y–X composition; the existing position/yaw API delegates to
that path. A combined-axis test verifies transformed skinned vertices. The
scheduled CommonCtr model-rebuild command uses this same renderer, rather than
recording a request without calculating geometry.

Room objects and pickups now decode through `decode_room_actor_models`, using
the original model-pointer ordering after enemy banks. Object slots 0–3 remain
reserved for player attachments. Objects can carry a length-prefixed SKIN block;
pickups carry raw MDL data. Object and pickup texture caches are separate and
keyed by authored ID, with repeated IDs reusing a pack without advancing the
texture-table cursor. Models without their own texture list do not inherit a
different prop's textures. Synthetic tests cover both caches, repeated object
and pickup IDs, reserved slots, skin prefixes and malformed bounds.
`bind_room_props` now binds these assets to scenario state, preserving player
attachment slots 0–3 while replacing room objects and pickups. It follows
`bhSetObject`/`bhSetItem`: low-byte type, high-byte parameter, authored floor,
signed three-axis angles, drawing type, hide bytes and flag-derived model flags.
Both replacement vectors are prepared before mutation, so malformed assets
leave the old room bindings intact. Native simple object and pickup controllers
now rebuild posed models in the shared runtime; special controllers and the
pickup highlight drawing overlay remain pending.
The headless retail scenario probe decoded 1,914 object/pickup models across
all 205 rooms, including nine skinned objects. The existing corridor lifecycle,
150-frame actor animation and scripted door probes also passed.

Step 1 is still incomplete: there is no playable application mode yet. Next is
integrating the new [movement/collision and camera simulation](player-movement-status.md)
into application input, original event-controlled room entry and animated
transition posing, followed by interactions, transitions, enemies, combat,
inventory, decoded audio and versioned save/load across the selected room pair.
