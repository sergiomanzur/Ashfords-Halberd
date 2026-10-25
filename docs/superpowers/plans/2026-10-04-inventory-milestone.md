# Inventory and item mechanics implementation plan

**Goal:** Restore source-derived item identities, carried inventory ownership, item operations and pickup persistence, with consistent menu/gameplay/save consumers.

**Architecture:** Preserve the original 384-word inventory and per-player carried banks. The equipped header selects a carried word; ammunition lives only in that word. Item metadata and combination rules come from pinned `item.c` and the consumers in `sub1.c`.

**Scope:** The inventory milestone authorized after the current project audit. Original inventory screen geometry/3D inspection, full campaign introduction, complete weapon animation/combat controllers and complete runtime save continuity remain separate Step 1 gates.

**Execution:** Main agent coordinates integration and headless verification; all spawned workers use GPT-6 Luna, Medium reasoning. Preserve other uncommitted work. No GUI, normal user profile, pushes, merges or publishing.

## Tasks and file ownership

- [x] Catalog worker: `src/item_database.hpp/.cpp`, `tests/item_database_tests.cpp`. Import all original 156 metadata entries, source weapon mappings, sprite mapping, stack limits, recovery and capacity attributes. Tests compare original table values rather than prototype assumptions.
- [x] Mechanics worker: `src/inventory_engine.hpp/.cpp`, `tests/inventory_tests.cpp`. Canonical active bank/selector; correct ownership through equip, swap, removal, ammunition consumption; exact supported original combinations and healing; transactional capacity and multi-slot allocation. Test duplicate guns, depleted ammo, full inventory, quantity overflow, poison and difficulty variants.
- [x] Pickup worker: new `src/item_pickup.hpp/.cpp`, `tests/item_pickup_tests.cpp`. Validate authored interaction and quantity; insert transactionally and set only authored persistence on success. Test full-inventory retry, non-ammo quantities and unrelated flag preservation.
- [x] Source combination worker: `src/inventory_combinations.hpp/.cpp`, `tests/inventory_combination_tests.cpp`. Adapt Combi_00 through Combi_05 and compare output against embedded upstream function bodies, checking the fixture against the pinned source.
- [x] Main integration: `src/runtime_view.cpp`, `src/inventory_ui.cpp`, `src/save_state.hpp/.cpp`, `tests/save_state_tests.cpp`, `tests/playable_slice_tests.cpp`, `CMakeLists.txt`, `src/item_use_area.hpp/.cpp`, `src/scenario.cpp`, `tests/item_use_area_tests.cpp`, `tests/inventory_event_tests.cpp`, `tests/runtime_inventory_tests.cpp`. Bind inventory type-0 floor areas and implement use-check/clear/item-loss event opcodes. Wire canonical ammo and pickup helpers; consume actual weapon index; align poison/conditions/sprites and menu useability; version saves for corrected representation, preserve player bank/difficulty, reject incompatible prototype saves and decode transactionally.
- [x] Independent review: Luna Medium reviews mechanics, source correspondence and integration; resolve findings and rerun affected tests.
- [x] Headless validation: build after worker edits settle; focused source/data/mechanics/save tests, then full CTest. Record any remaining failures without weakening tests to manufacture slice acceptance.
- [x] Documentation: update current inventory status and Step 1 status with measured evidence. Keep historical audit checkpoints clearly dated.

## Completion criteria

1. Catalog and inventory mechanics use original item IDs and source tables, with honest labels where localized item names are not decoded.
2. Re-equipping cannot restore ammunition; duplicate weapons and swaps retain individual slot ownership.
3. Capacity, two-slot records, reload/combination/healing/poison and quantities agree with the original source for implemented inventory operations.
4. Failed pickups do not consume world items or persistence bits; successful pickups use authored quantity and identity.
5. Menu, gameplay and versioned save/load agree on the same bank, equipped selector, ammunition and poison state.
6. Source-derived headless regression checks pass. Overall Step 1 remains incomplete until its independent original-game acceptance gates pass.

## Outcome

Scoped inventory mechanics implemented; see [measured results](../../inventory-milestone-2026-10-04.md).
Eight inventory/save checks pass; full CTest is 166/167 with the original-zombie
encounter acceptance gate still failing. This does not complete Step 1, visual
parity or full runtime save continuation.
