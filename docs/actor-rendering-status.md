# Native animated actor rendering — 2026-10-02

The core now builds player joint matrices using the reference `bhCalcTree`
translation/ZYX rotation path. It adjusts skin vertices for bind translations,
blends the unrotated/rotated joint transforms using the reference ten-step
weights, applies actor position/yaw, and bypasses the skin-owner drawing matrix.
Rigid meshes use their posed joint matrices. Actor object indices are kept
outside the room camera's hide-mask range.

This follows the default `npSkinConvert`/`npCalcSkin` path. FM remapping,
facial variants, attachments, lighting
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

Step 1 is still incomplete. The main-menu corridor preview now connects native
movement, animated actors and original room cameras to application input and
rendering. Interactions, connected-room transitions, enemies/combat, inventory,
decoded audio and versioned save/load across the selected room pair remain to
implement before the complete playable slice is ready.

`actor_joint_matrices` now exposes world matrices in model joint order, using
the same validated hierarchy traversal as rendering. Matrix-only queries do
not decode meshes, skin buffers or textures. `pose_actor_linked` follows
`bhCalcTree`'s baseline parent-joint matrix, local translation and Z–Y–X
rotation composition. It accepts finite affine parent matrices and preserves
cached model/skin bytes. The optional work flag `0x1000` rotation reset and
subtree selection remain pending. Body channel application is described below.
This API poses a supplied whole model; it does not initialize zombie body-part
controllers or make the graveyard playable.

Synthetic checks verify noncommuting parent/local transforms, combined-axis
rotation, child/sibling hierarchy, rigid drawing agreement, immutable inputs
and malformed parent/skeleton rejection. The headless retail runtime probe
verified 30 skeleton samples and 180 whole-model linked placements for ten
graveyard zombie roots across both room variants, comparing every triangle
corner with its parent joint placement and checking cached asset immutability.

Motion application now handles the original full/body destination maps, body
joint offset, mirrored axes and work flags before rendering. All six
Normal/Fast/Slow direct/blended paths preserve their branch-specific absent
channel behavior. Normal and Slow direct paths clear only missing angle X;
SlowHokan retains Y/Z in the shared blend-angle scratch. These unusual stores
were verified against the user's USA ELF. Only FastHokan applies flag 8's
alternate Euler conversion. Generic raw sampling keeps absent channels absent.
The corridor controller and player locomotion both use this bounded application
helper, sharing scratch owned by `ScenarioState`, including initial room entry.
Errors leave caller motion/pose inputs and shared scratch unchanged.

The headless retail probe applied 480 full/body startup and walking samples
across ten graveyard roots, checking mirroring, blending and the body offset
against both model versions. Body samples preserved joints before index eight;
direct samples were checked against independent destination/axis mapping.
This completes the channel-application dependency, not zombie initialization,
body subtree drawing, AI/combat or the complete playable slice.

Selected root/body startup now uses owner-index pose sharing and world joint
matrices for its collision capsule. Attached-body initialization refreshes the
parent's combined rendered pose; retail checks cover all ten roots across the
two graveyard variants. The root's requested material color and both shadow
descriptors remain metadata until their original rendering paths are adapted.
Detached body controllers and their model/skin rebasing remain required before
complete enemy rendering. General saved-matrix/range support is described below.

Persistent `ActorModelWork` now implements the model-calculation dependency.
It owns O_WORK flags and matrices in full asset joint order. Calculation walks
the selected contiguous range in the source's linear order, pushes matrices
for siblings, pops them at leaves, and restores flag-1 world matrices before
processing descendants. Work outside the selected range is retained. Linked
placement transforms the local offset before flag-0x1000 resets rotation and
applies XYZ angles. Unlinked flag-0x1000 retains the preceding base matrix.

`pose_actor_model_work` draws selected mesh owners with these cached matrices,
using original full-model skin bindings. Selecting a joint range does not
guarantee a mesh in that range: the retail zombie's entire skin mesh can be
owned by joint zero. Controllers that select/rebase separate skin lists still
need their own source path; this API does not claim that rebasing. Visibility
flags and immutable model/skin bytes remain respected.

Zombie bind initialization and attached-body capsule/render refresh now use
this persistent work directly, avoiding a second divergent matrix cache.
Synthetic checks cover frozen child/descendant/sibling behavior, reset/retained
placement, selected geometry, frozen skinned vertices, invalid work and copied
runtime isolation. Material/shadow rendering and actual detachment, damage and
enemy controller integration remain pending.

Native facial model preparation now adapts the selected graveyard ID 91
dependency from `face_bh.c` and `njplus.c`. It converts the original ordinary
skin in private storage before inserting tooth, tongue and eye objects.
Model-bearing DFS lookup suppresses descent into already wrapped parents.
Source face/tongue vertices remain separate from their mutable destinations;
wrappers retain the original clone angles and scales. Jaw anchors initialize
parent Y/Z rotations using the source atan ratios. New O_WORK entries mask
motion on joints 6 through 14.

The expanded model explicitly selects FM skin decoding: runtime joints 11–14
consume no source records, and packed influences above 10 receive index +4.
Expanded rendering requires preconverted vertices. Joint indices use 16 bits
so packed index 255 can map to 259 without wrapping. Absolute world skin output
and identity drawing matrices apply placement once, equivalent to the original
actor-relative skin output followed by its actor translation.

Synthetic tests cover immutable input, private output, malformed headers,
IDs/tables/graphs/vertices, four-object expansion, fractional rotation blending,
angled jaw anchors, and translated/rotated world placement. The headless retail
probe prepares room archive entry 2 slot 4 from 27 to 31 joints and draws six
meshes. This preparation intentionally supports the selected four-added-object
skin layout. Facial deformation and eye matrices are described below. Lip
shapes and MASK/controller updates remain required before ID 91 runs in gameplay.

Normal facial shape keys now decode from the original MASK variable-table
layout into native `FaceParameters`. Sampling follows the source unsigned-frame
selection and first/last clamping. The five interpolation modes preserve
selective muscle updates, jaw/tongue level scaling and unscaled eye coordinates.
Cosine interpolation uses the existing Ninja quarter-wave table and its 16383
reflection, including integer angle truncation. The graveyard MASK has 24
normal shape keys ending at frame 46. Packed lip keys and controller integration
remain pending. These normal parameters feed the deformation path below.

Selected mode-1 facial deformation now decodes native muscle connections and
weighted jaw/tongue bindings. It resets only the source's designated vertices,
accumulates muscle contributions in the PS2 assembly's reverse scratch order,
and applies aligned jaw rotation and translation. Fractional jaw rates scale
rotation while retaining the full jaw translation. Tongue Z stretch precedes
its per-rate X/Y rotation. Eye matrices are captured during original wrapping,
with XYZ scale, before jaw alignment; eye pitch/yaw retain signed 32-bit stores.
The original selected mode 1 keeps source normals.

Updates stage all model bytes before committing, so malformed parameters or a
late eye failure cannot commit earlier muscle/jaw writes. Aggregate MASK byte
extents are checked before native list allocation. Tests verify signed eye
angles, captured scale, tongue stretch, repeated-frame stability, immutable
source vertices, world placement and failures with constrained allocation.

The retail probe samples the full 24-key shape track at half-frame intervals,
deforms and draws each of 93 frames with native persistent model work, and
checks actual triangle movement. This is the `fmCnkCalcFace` dependency, not
the complete `bhControlMask` path: scheduler gating, lip data and its specific
copies/inverted eye angles into the runtime joint table remain to be adapted.
