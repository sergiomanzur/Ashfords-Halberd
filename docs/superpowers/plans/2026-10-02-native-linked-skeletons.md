# Native linked skeletons implementation plan

Continue the full Step 1 scope. Zombie body parts need the animated parent
joint matrices used by MdlPut.c::bhCalcTree. Game startup and complete enemy
controllers remain required; diagnostic linked poses cannot complete them.

1. Expose immutable world joint matrices from the existing validated actor
   skeleton traversal. Reuse exactly the same transform math for drawing.
2. Add linked placement from parent joint matrix, local offset and local XYZ
   rotation. Validate malformed parent matrices before changing any asset data.
3. Add synthetic checks for parent rotation/local offsets, hierarchical joint
   matrices, scene agreement and immutable inputs. Preserve all existing skin
   and actor rendering checks.
4. Verify native matrices and linked placements against retail graveyard model
   resources headlessly, with nonzero coverage. Review source/order fidelity.
5. Run the full build/tests and packaged smoke; record actual limits and commit.
   Motion mirroring and partial-body channel application are the next dependent
   work before the complete zombie initialization/controller can run.
