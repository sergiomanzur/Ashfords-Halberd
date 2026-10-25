# Step 1 completion roadmap (2026-10-04)

Goal: finish the Step 1 playable slice per
`docs/superpowers/specs/2026-10-02-windows-port-design.md`.

Current status audited 2026-10-04: Step 1 remains incomplete. The development
build succeeds; after the scoped inventory mechanics rewrite, 166/167 CTest
entries pass, with playable-slice-verification failing at original zombie
encounter verification. The eight inventory/save checks pass. New Game is enabled as an experimental prototype.
The former 156/156 and disabled-menu baseline was an earlier checkpoint.
See [current status](../../step1-slice-status.md) and
[audit](../../project-audit-2026-10-04.md). The tasks below are requirements,
not evidence of completion; prototype implementations do not satisfy them.

Each increment is complete only when it has headless tests, the full
`./tools/build.ps1` run passes, and its limits are documented. No placeholder
or silent no-op services.

## 1. Armed player binding

Upstream: `weapon.c::bhSetWeapon/bhObjWpn/bhActionWeapon`, `player.c` armed
states. Integrate decoded weapon objects, `WpnAngTab` angles, owP[7..13]
visibility and the original armed motion/controller ownership. Relaxing an
equipment guard does not establish this behavior; preserve unrelated invalid
player-ID checks. Tests: source flag/angle tables, equip/unequip, actual weapon
attachment, armed poses and joint visibility.

## 2. Aiming, firing, ammo

Upstream: `player.c` aim/shoot/reload modes, `weapon.c::bhCheckBullet`,
`bhCountBullet`. Needs armed motions from the player package and ammo state
in `ScenarioState`. Tests: mode transitions, ammo decrement, reload bounds.

## 3. Hit detection and damage

Upstream: `weapon.c::bhCheckGunAtari`, `EneDamNear/Mid/Far`,
`bhSetGunSplash`. Integrate with existing zombie capsules and defeat
persistence. Tests: range bands, per-part damage, zombie death and
persistence across room reload.

## 4. Interaction controllers

Port original examination, standing/kneeling pickup, item-box and stair/ledge
controllers required by the two selected rooms; fail explicitly on unsupported
consumers. The current view-level pickup shortcut is not acceptance evidence:
it forces quantities, searches beyond carried capacity and does not make world
item consumption conditional on successful insertion.

## 5. Inventory

Upstream: `item.c`, `itemview.c`. Native inventory data model and screen,
driven by pickups, with keyboard/controller navigation. Tests: add/remove,
stack limits, equip hand-off to increment 1.

## 6. New Game loop

New Game is already enabled in the prototype. Finish its fixed-tick runtime,
original camera, interactions, door transitions (stage 0 room 1 -> room 2 is
the authored connection), genuine enemy encounter, inventory and audio in one
session. Removing an "in development" label did not satisfy this requirement.

## 7. Save/load

Upstream reference: `ps2_McSaveFile.c`. Versioned native save under the
user-data root with backup/recovery like the INI. Tests: round trip across
both rooms, truncated/corrupt files, version mismatch.

## 8. Acceptance

Full suite and ordinary-input slice acceptance against an independently
identified original-game reference. Automated checks remain headless per the
user's instruction; no desktop playthrough is authorized by this plan.
An independent reference has not yet established acceptance. Document current
results in `docs/step1-slice-status.md`, distinguishing component tests, rendering
captures and actual original-gameplay equivalence.
