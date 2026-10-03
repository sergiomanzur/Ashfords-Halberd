# Native model-work calculation and drawing

This implements the saved-matrix/range dependency in the already authorized
native zombie initialization plan. Step 1 remains the complete playable slice.

1. Test `bhCalcModel` placement: parent matrix followed by local translation,
   optional linked flag-0x1000 rotation reset, then XYZ rotation; unlinked
   flag-0x1000 retains its prior base matrix, ignoring position/angle inputs.
2. Represent O_WORK flags/matrices in an owned vector, preserving work outside
   a selected contiguous joint range. Port `bhCalcTree`'s linear model order,
   sibling matrix pushes, leaf pops and flag-1 saved world-matrix restoration.
   A saved matrix must affect children but not siblings. Fail safely on invalid
   ranges, model hierarchy or saved matrices; immutable source bytes stay intact.
3. Draw only the selected linear model range using calculated work matrices,
   including existing skinning and visibility handling. Validate full work
   vectors and keep skin bind conversion independent of frozen current poses.
4. Connect ordinary attached zombie capsule/render updates to persistent model
   work, preserving shared pose ownership and transactional startup behavior.
5. Review source fidelity; run synthetic renderer/startup tests and the full
   Windows build, retail probes and packaged executable smoke headlessly.

This does not finish zombie damage/detachment controllers, source skin-list
rebasing, material/shadow rendering, AI, attacks or ID-91. Those remain required
before actual graveyard travel and complete Step 1.

## Implemented checkpoint

Persistent native joint work, linear matrix calculation, selected mesh-owner
drawing and the zombie capsule/render integration are implemented. Tests cover
saved matrices affecting descendants without contaminating siblings, frozen
skinned vertices, selected ranges, retained outside work and placement flags.
The source reviewer found no actionable defect in the represented path.

The retail probe passes 480 linear matrix comparisons against the independent
hierarchy query and 20 frozen prefix/suffix drawing comparisons across all ten
graveyard roots. A suffix has no mesh when the entire skin mesh belongs to
joint zero; the probe compares actual owner ranges instead of requiring a mesh
in each range. Original full-model skin bindings remain explicit. This does
not prove detached-body skin-list rebasing or an enemy encounter.

Final headless validation: `tools/build.ps1` passed all 19 CTest entries and
created the Windows package. The retail runtime probe validated all 205 rooms,
480 model-work samples and 20 selected drawing ranges, retaining startup and
960 corridor runtime-tick coverage. The packaged smoke rendered 240 frames,
applied movement, exercised focus-loss blocking and returned to the main menu
under SDL dummy video/software rendering with an isolated profile.
