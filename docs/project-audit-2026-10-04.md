# Current project audit — 2026-10-04

**Historical checkpoint:** this audit preceded the inventory mechanics rewrite.
For subsequent fixes and measured results, see the [inventory milestone report](inventory-milestone-2026-10-04.md) and [current Step 1 status](step1-slice-status.md).

## Assessment

Step1 remains incomplete. The working tree contains a native prototype with
retail asset rendering, an enabled New Game entry, inventory screens, simplified
shooting/pickups, and versioned saves. These additions are substantial, but the
original gameplay consumers and UI fidelity are not fully implemented. The
previous completion assertion was contradicted by fresh test results and
production controller guards; the current Step 1 report corrects it.

At this audit checkpoint, HEAD is 3037798. The subsequent effect-resource work and the other model's
UI/combat/save changes are all uncommitted. Existing edits were preserved.
The audit changed no production code, tests or build configuration. This report
was promoted into project documentation after the user requested factual status.

## Fresh headless verification

- `cmake --build build/windows --parallel 4`: successful.
- `ctest --test-dir build/windows --output-on-failure`: 160/161 pass.
  `playable-slice-verification` fails at "Slot 1 is ammo (handgun bullets)";
  RuntimeView stores a duplicate gun there and ammunition in slot2.
- Current AshfordGameplayCapture executed with dummy video/audio/software and
  an explicitly supplied fresh ignored output directory. Four frames captured.
  The capture helper teleports the player for its door test and catches errors,
  so its success is rendering evidence, not natural traversal acceptance.
- Private mechanics probes reproduced retained actual poison after blue herb,
  ammo restoration by re-equipping, unsupported zombie hit/mode1 exceptions,
  and two malformed-effect resource acceptance defects.
- Actual second save/load succeeded with HP90. A separate rename-overwrite
  probe also succeeded. The candidate Windows overwrite bug was disproved and
  removed from the save auditor's findings.

All checks used headless execution and ignored local profiles/files. No GUI,
browser, audible output, normal user profile, push, merge or publication.

## Priority findings

1. **Real zombie combat remains blocked.** zombie_controller.cpp:16-36 rejects
   nonlethal hit processing and main modes1..4. New weapon damage sets hit flag4
   then reaches this guard. Lethal damage hides the actor and scenario.cpp:1100
   skips flags2 entirely, bypassing the original death sequence. Defeat indices
   are assigned by room constants. The integration test accepts scripted actor67
   as a zombie, patches HP and teleports actors, so it does not prove an original
   id1 encounter. Source inspection: `src/zombie_controller.cpp`, `src/weapon.cpp`,
   `src/scenario.cpp` and pinned `en01.c`; private detailed audit retained.

2. **Inventory header/weapon ownership is inconsistent.** Original element0
   selects the equipped carried slot; new code instead stores a packed item/ammo
   mirror. RuntimeView decrements that mirror, while the carried gun retains15.
   Re-equipping copies the unchanged gun back and restores ammunition. Original
   inventory item IDs and weapon numbers are also conflated in the catalog.

3. **Item mechanics are not source faithful.** Poison treatment/UI inspect
   system flags[10] bit0x4000 instead of player_state_flags bit0x80000. Herb IDs27
   and28 are reversed, red herbs can be consumed alone, difficulty320-HP recovery
   is absent, condition thresholds and multi-slot capacity differ. Catalog and
   sprite mappings are incomplete. Source: `src/inventory_engine.cpp`, `src/item_database.cpp`,
   pinned `item.c` and `sub1.c`.

4. **Pickup ownership is incomplete.** RuntimeView grants30 units for every item,
   searches all384 words rather than the current carried capacity, wraps amounts
   and applies a constant pickup persistence mask. Consumption is not conditional
   on a successful insertion. This can store inaccessible items or lose pickups.

5. **Save restore does not establish full continuity.** Serialization omits RNG,
   room variants, actor damage/state, tasks, effects and stream positions. Load
   runs normal entry initialization before restoring position, masks saved flags,
   then commits before fallible audio/setup/tick work. Failure can replace live
   state; existing tests omit cross-room restore and these continuity assertions.
   Source: `src/save_state.cpp` and `src/runtime_view.cpp`. Overwrite works on the current toolchain.

6. **UI is a custom composition using retail texture fragments.** Item cards,
   descriptions, fonts and fixed1120x720 placement do not establish1:1 source
   layout. Expanded slots9/10 overlap the description panel. FILE/MAP are painted
   tabs; CHECK is a static sprite/prose modal without original3D inspection and
   puzzle hotspots. An independently identified original-game reference frame is
   still needed for pixel-level acceptance. Current native capture alone does
   not provide that comparison.

7. **Resource validation still accepts broken data.** A global MDL object model
   pointer outside its chunk is accepted. Removing the terminator before the
   next room pack makes one family consume both packs, shifting family indices,
   without rejection. These are reproduced in the private mechanics probe.

8. **Script/effect acceptance remains unfinished.** Opcode173's assignment is
   source correct, but the full original effect controller/drawing pass is not
   connected. Sound instructions134/135 also bypass original event semantics.
   Independent original-game audio/visual/gameplay comparison remains required.

## Next sequence

Restore canonical item IDs/slot selection and source-derived startup; integrate
the real player/weapon/zombie state machines and effect consumers; make pickups
and saves transactional; then reproduce original UI geometry/assets/inspection
and validate actual traversal/encounter/save continuation against the original.
Rework acceptance tests so they exercise ordinary input and source behavior,
not teleported/patched state or invented item rules. Keep Step1/New Game maturity
claims aligned with those gates rather than the count of component tests.

## Evidence availability

The commands and reproduced outputs are summarized above. Detailed logs,
synthetic probe code, user-owned assets and captures are local/ignored and
may not exist in another checkout. They are not distributed reference evidence.

## Local evidence

- local/current-project-audit-build.log
- local/current-project-audit-ctest.log
- local/current-project-audit-capture.log
- local/current-project-audit-capture-path.txt (fresh screenshot directory)
- local/current-mechanics-audit-probe.cpp/.log
- local/current-save-rename-probe.cpp/.log
- local/current-ui-audit.md
- local/current-gameplay-audit.md
- local/current-save-audit.md
