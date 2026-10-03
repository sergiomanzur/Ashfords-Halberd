# Native graveyard enemy initialization plan

Step 1 remains the complete native playable slice. Current channel application
is verified; actual graveyard travel still fails on ID 1 at mode 0. Implement
the selected room's original enemy startup and continue through its required
movement, combat and ID-91 controller rather than enabling a static encounter.

## Source constraints for the next implementation

- `en01.c::bhEne01_Init` creates six core linked works and, for the first
  qualifying root, an additional ID-0x26 manager. `bhEne01_FastCheck` chooses
  the first active, unlinked ID-1 root whose type is not 10. The manager clears
  its parent link/linked flag and remains hidden. Six diagnostic bindings per
  root do not prove complete startup.
- `bhEne01_SetLinkEnemy` binds `epp->mlwP` to the parent's `mdl`. Attached body
  work therefore applies motion to shared mutable joint state, beginning at
  joint 8, while retaining its own motion counter/control fields. Native
  indices must route those updates to the parent pose; sharing immutable model
  bytes alone is insufficient. Copying a runtime must not alias another
  runtime's mutable pose. Detachment needs the original separate-work path.
- `bhEne01_CalcEnemy` calls `bhCalcModel(parent)` for attached linked work.
  Detached body work temporarily selects the joint-8 suffix and calculates
  that suffix separately. Capsule endpoints/heights derive from the resulting
  joint matrices, not guessed character dimensions.
- `MdlPut.c::bhCalcTree` respects O_WORK flag 1 by restoring that joint's saved
  world matrix into the traversal. Linked placement flag 0x1000 clears the
  rotation portion after local translation. Unlinked flag 0x1000 retains its
  existing base matrix. Drawing iterates the selected linear model-work range.
  The existing whole-model pose APIs do not implement these cases yet.
- `en01sub.c` head/arm/leg startup sets state flags, hidden linked-work modes
  and separate extended counters. The parent manager initializes separately,
  then performs nearest-root and stair checks. These are actual scheduler
  dependencies, not optional diagnostic callbacks.
- Selected authored roots are type 0, model versions 9 and 32; ID 91 is also
  active. Other model-specific cap/worm/bomb attachments require explicit
  support when reached. Do not silently substitute selected-room behavior.

## Execution

1. Write source-based tests for attached pose ownership, copied-state isolation,
   first-root manager allocation, reset/reinitialization order and RNG cadence.
2. Add explicit native extended-work fields/indices and finish main startup,
   combining verified health/type/motion helpers with linked work and flags.
3. Implement the required saved-matrix/range behavior in the renderer with
   independent transform tests, then capsule calculation from joint matrices.
4. Complete head/arm/leg/manager startup and original dispatch ordering.
   Exercise these against both real graveyard variants headlessly.
5. Implement the room's ID-91 startup/control, zombie movement, attacks/damage
   and player response before enabling travel in the application preview.
6. Verify the actual transition, encounter and return path, then integrate
   inventory, decoded audio and versioned cross-room save/load required by
   Step 1. Keep New Game unavailable until the approved playable-slice scope
   and original-reference behavior comparison pass.
