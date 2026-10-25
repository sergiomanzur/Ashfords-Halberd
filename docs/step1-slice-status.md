# Step 1 status — audited 2026-10-04

**Step 1 is incomplete.** The current working tree is an experimental native
Windows prototype. New Game is enabled and opens that prototype; its availability
does not establish completion or original-game compatibility.

This is the current project status reference. Older component reports describe
their own checkpoints. Their test counts, disabled-menu statements and script
boundaries must not be read as current application status. The
[Windows design](superpowers/specs/2026-10-02-windows-port-design.md) remains the
acceptance specification. See the [dated audit](project-audit-2026-10-04.md).

## Latest verification

After the scoped [inventory mechanics milestone](inventory-milestone-2026-10-04.md),
headless checks use SDL dummy video/audio, software rendering and isolated data:

- `cmake --build build/windows --parallel 4`: succeeded.
- Inventory/save checks: **8/8 passed**, with no asset-dependent skip.
- Full `ctest --test-dir build/windows --output-on-failure`: **166/167 passed**.
- `playable-slice-verification` passes starting inventory and locomotion, then
  fails: `Original zombie encounter required; scripted actor 67 is not a zombie`.
  It no longer accepts a scripted actor as proof of original zombie combat.
- Source catalog values and combination outputs are checked against the pinned
  decompilation. Live tests confirm ammunition ownership and controlled cross-room
  inventory/poison/difficulty restoration; enemy updates are disabled in that
  isolated inventory test. These are not original-playthrough acceptance.
- The earlier [audit](project-audit-2026-10-04.md) is a historical checkpoint.
  Its inventory/ammo/poison/pickup defects have been addressed by this milestone;
  its combat, effects/resource validation and visual-fidelity findings remain
  outside this work. No original-frame comparison was completed here.

The milestone did not certify tools/build.ps1 or a fresh installed/ZIP release.
The packaging script stops when CTest fails. Private assets and logs stay ignored.

## Implementation and remaining work

| Area | Present | Remaining problems or acceptance |
| --- | --- | --- |
| Windows foundation | Native executable, user-owned USA ISO import/cache, menu and INI settings | Earlier foundation checks do not certify later gameplay changes. |
| Presentation | Retail asset decoding, camera cuts, animation and component rendering | Original camera/effect paths and independent visual comparison remain unfinished. |
| Gameplay entry | New Game opens a corridor prototype with a supplied starter loadout | Original initialization and faithful two-room progression are not established. |
| Combat | Weapon tables, geometry helpers and simplified aim/fire/damage code | Real zombie modes 1–4 and nonlethal damage throw. Death is bypassed; original weapon state, timing, pose and persistence need integration. |
| Inventory | Source catalog, canonical bank/selector/ammo, extra slot, weighted capacity, combinations and recovery; consistent menu consumers | Localized names remain undecoded. UI is not 1:1; FILE/MAP, original CHECK, dialogue/sound/animation fidelity are incomplete. |
| Interactions | Authored transactional pickup grants, type-0 inventory use areas and item-use/loss script handshakes | Original presentation and complete puzzle flow remain unverified. Natural traversal remains unverified. |
| Effects/audio | Native resources/audio services and opcode 173 assignment | Full effect scheduling/drawing and gameplay sound consumers remain unfinished; reproduced resource-validation gaps remain. |
| Saves | Version-2 inventory/status saves, active player/difficulty/spray count, atomic decoding and replacement-room preparation; pending item transactions rejected | Controlled cross-room inventory checks pass. Complete actor/task/effect/RNG/audio continuity and natural-playthrough recovery remain incomplete. |

## Completion gate

Step 1 requires the original two connected rooms, player/enemy behavior, camera,
interactions, combat, inventory, decoded sound/effects and versioned cross-room
save/load to work together. Acceptance must use ordinary gameplay input and an
independent original-game reference. Tests that patch health, teleport actors,
accept a scripted actor as a zombie or silently succeed without assets do not
meet this gate. Component test counts and the New Game button are not completion
metrics.

No measured completion percentage is available. Previous rough percentages were
effort estimates, not an acceptance metric. The full campaign and optional RE2
Remake style over-the-shoulder camera remain Step 2 work; other platforms remain
future scope.
