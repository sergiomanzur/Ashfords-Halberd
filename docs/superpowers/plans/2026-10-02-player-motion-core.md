# Player model and motion dependency

This implements the next dependency of the approved native playable slice;
the full Step 1 acceptance criteria remain in the Windows port design.

1. Decode MTN headers and dense translation/angle keys without realizing disk
   offsets as host pointers. Test malformed extents and angle wraparound first.
2. Decode the player package's length-prefixed model/skin, motion, auxiliary
   and aligned texture blocks, preserving absent motion slots and model links.
3. Exercise the native readers against the user's default Claire package in
   SYSTEM.AFS entry 10; never distribute retail bytes or extracted data.
4. Adapt skin binding, joint transforms and animated actor rendering, then
   integrate player movement and room collision with the original game logic.
5. Select the connected two-room encounter and implement interactions, combat,
   inventory, sound and versioned save/load; verify the actual full slice.

All runtime verification uses dummy SDL video or file-only diagnostics. No
interactive window is authorized during development checks.

Steps 1–3 are implemented and verified. The ninth CTest suite covers motions,
package records and skin bindings. All nine suites passed headlessly; the
decoder suite also passed with standard-library assertions enabled. Review
prompted cumulative key budgets and exact mixed aligned/ordinary texture
association tests. Retail entries 10–14 parsed and sampled successfully.

Step 4 now includes joint transforms, default-path skin deformation and textured
actor rendering. File-only native previews combine Claire with imported room
geometry and its camera/depth buffer. Fixed-tick application gameplay and the
remaining Step 1 requirements are still pending. Ten headless suites verify this
dependency, including malformed skeletons, skin groups and matrix composition.
