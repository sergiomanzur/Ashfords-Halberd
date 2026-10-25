# Native inventory status — 2026-10-04

Inventory mechanics have been rewritten around the pinned original source.
The screen remains a custom native UI and is **not** an accepted 1:1 reproduction.
Retail texture fragments and passing mechanics tests do not establish visual parity.
See the [inventory milestone report](inventory-milestone-2026-10-04.md),
[current Step 1 status](step1-slice-status.md) and
[historical audit](project-audit-2026-10-04.md).

## Current implementation

Canonical item/variant IDs, the active player's equipped selector, separate extra
slot, carried ammunition, weighted capacity and original combination rules now
feed the menu and gameplay. Recovery uses actual player poison and source health
rules. Pickup grants are transactional and authored scripts commit persistence.
Save version 2 retains inventory ownership and rejects old prototype saves and
pending item transactions. Localized names/descriptions remain undecoded and are
explicitly labeled as such.

The menu supports keyboard/controller browsing, equip/unequip, source recovery,
room-matched item use, combination selection and static examination. X or LB
selects the extra slot. Sprite coordinates come from ItemBoxIconSet rather than
invented item mappings. The description panel no longer covers expanded slots.

## Fidelity gaps

The screen uses fixed 1120x720 placement, item cards, mixed text rendering and an
added description panel. FILE/MAP are decorative. CHECK shows a sprite and prose;
original 3D rotation, zoom, inspection hotspots and puzzle interactions remain
absent. Original geometry and independent reference frames are still needed for
visual acceptance. Combination dialogue presentation and original sounds/animation
are not established by scalar mechanics coverage.

## Verification

See the milestone report for the coordinated headless build/test results and
coverage limits. Earlier prototype assertions and the audit's 160/161 count are
historical checkpoints; they do not certify the rewritten implementation.

This work does not complete Step 1 or certify a packaged release.
