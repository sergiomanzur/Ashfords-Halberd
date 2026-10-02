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

Step 1 is still incomplete: there is no playable application mode yet. Next is
integrating the new [movement/collision and camera simulation](player-movement-status.md)
into application input, original event-controlled room entry and animated
transition posing, followed by interactions, transitions, enemies, combat,
inventory, decoded audio and versioned save/load across the selected room pair.
