# Inventory mechanics milestone — 2026-10-04

This report covers the scoped inventory/item mechanics work, not completion of
Step 1 or acceptance of a 1:1 inventory screen. The application remains an
experimental native Windows prototype with a supplied room-1 starter loadout.

## Implemented behavior

- All 156 original ITEM_WORK rows, all four getbulletmax columns, all 410
  combination-table values, WeaponSet item/variant mappings and ItemBoxIconSet
  atlas coordinates come from the pinned decompilation.
- The active player owns a 16-word bank: word 0 is an equipped local-word
  selector, word 1 is the separate extra slot, and carried records begin at
  word 2. Eight/ten capacity counts original two-slot item attributes.
- Shooting consumes ammunition in the selected carried record. Duplicate guns,
  swaps, empty weapons, the extra lighter and infinite ammunition retain their
  own state; re-equipping does not refill ammunition.
- Source combination helpers handle reversed dispatch, reserve conversions,
  grenade/bow variants, powder arithmetic, upgrades and quantity overflow.
  Pickup reserve caps and combination loaded caps are distinct rules.
- Recovery uses original curedata, normal/320-HP modes, actual player poison
  and chronic-illness rules. Spray use updates a persisted usage counter.
- Inventory use resolves active type-0 static/dynamic floor areas using original
  bounds, facing and front-probe rules. Matching items raise the script request;
  original use-check, use-clear and item-loss opcodes consume it.
- Pickups validate the current authored trigger/item, grant source quantities
  and room-specific variants, preserve zero-quantity key records, and insert
  transactionally. Event 14 commits the
  authored world/persistence update after acceptance. Failed grants retain the
  item and unrelated progress flags.
- Save version 2 preserves inventory banks, selector, active player, difficulty,
  player poison/health, room resource variants and spray count. Old version-1
  saves are rejected. Decoding and room reconstruction preserve live state on
  failure. Saves reject pending item transactions rather than capture a grant
  before its world/persistence commit.
- The menu reads the canonical inventory, offers equip/unequip/use/combine,
  exposes the extra slot through X/LB, and closes for successful script-driven
  item requests. Original sprite atlas coordinates replace guessed mappings.

## Verification

The scoped mechanics milestone is implemented. Headless validation on 2026-10-04
uses SDL dummy video/audio, software rendering and an isolated LOCALAPPDATA.

- `cmake --build build/windows --parallel 4`: succeeds.
- Inventory/save selection: **8/8 pass**, with no skipped asset check.
- Full CTest: **166/167 pass**, with no skipped tests.
- Remaining failure: `playable-slice-verification` now passes initial inventory
  and locomotion, then rejects actor 67 as an original zombie encounter. This
  remains a broader Step 1 combat/acceptance gate; the test is not weakened.

The eight checks cover save round trips/failed decoding, mechanics and menu
input, catalog equality against original item.c, a combination oracle, authored
pickups/events, live RuntimeView inventory/save reconstruction and type-0 use
areas. Assertions remain active with NDEBUG. The combination oracle verifies
its embedded Combi_00..05 bodies against the pinned sub1.c before comparing
success and output words over more than 1,000 recipe/mode/quantity/variant cases.

The live runtime check disables enemy updates to isolate inventory consumption,
uses user-owned retail assets, and constructs a cross-room save fixture. It does
not establish natural traversal or original combat. It also verifies that an
unavailable saved room leaves the live room/inventory intact. Tests reject saves
with pending pickup/use transactions to avoid splitting inventory from progress.

Logs remain ignored: `local/inventory-milestone-configure.log`,
`local/inventory-milestone-build.log`, `local/inventory-milestone-focused.log`
and `local/inventory-milestone-full.log`. Full packaging through tools/build.ps1
and a fresh installed/ZIP release are not certified while CTest has a failure.

## Remaining limits

Localized item names/descriptions are not decoded: labels explicitly say
"Item N (name not decoded)". Magazine capacity and compatible-ammunition fields
are not invented from ITEM_WORK. The source combination corpus is finite
regression coverage, not a complete original-game playthrough.

The inventory screen still uses a custom layout. FILE/MAP remain decorative;
CHECK does not implement original 3D rotation, hotspots or inspection puzzles.
The type-0 floor service does not claim to implement floor sound, water or ladder
controllers. Original introduction/starting inventory, armed animation and full
combat are separate requirements. Saves still lack complete actor/task/effect,
RNG and audio continuation; controlled cross-room inventory tests do not prove
natural gameplay traversal or complete save continuity.

See [current Step 1 status](step1-slice-status.md), the
[historical audit](project-audit-2026-10-04.md) and the
[implementation plan](superpowers/plans/2026-10-04-inventory-milestone.md).
