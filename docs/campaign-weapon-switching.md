# Weapon switching — 2026-10-24

Implements the carried-weapon QoL requested from [upstream issue #67](https://github.com/aikatore/recvx-enhanced-port/issues/67).

| Default | Action |
|---|---|
| `[` / `]` | Previous / next carried weapon, with wrapping |
| F3 / F4 / F6 / F7 | Carried weapon 1 / 2 / 3 / 4 |
| L3/LS + D-pad Up / Right / Down / Left | Carried weapons 1 / 2 / 3 / 4 |
| L3/LS + LB / RB (PlayStation L1 / R1) | Previous / next carried weapon |

All six rows are global to fixed-camera and third-person controls and appear
in the keyboard/mouse and gamepad binding editors in both F1 and native
Enhanced Options. Each action supports the existing two binding positions and
INI persistence, including controller combinations. To rebind a combination,
hold L3 and press the other input during capture; releasing L3 by itself binds
just the stick click. Combination labels use the connected controller family.
Version 3 upgrades the previous empty controller defaults and L3-only cycle
default; custom assignments and explicit version-3 unbinding persist. Assigning an occupied key uses the existing conflict swap.

Direct ordinals follow the current inventory grid, rather than storing a slot
pointer that could later refer to ammunition or a key item. Removing, sorting
or storing equipment changes the order. Distinct carried copies remain distinct
entries. A two-cell item occupies one packed inventory word, so the following
word remains independently selectable; the visual second cell adds no entry. Normal capacity ends at
inventory word 9; expanded capacity at word 11, using the native game-mode flag.
The separate lighter slot and item boxes are excluded. Retail item flags identify
weapons; zero ammunition does not make a weapon disappear from selection.

Only lowered, compatible standing/turning/walking/running/backstep states and
the native idle animation can switch. Aiming, raising/lowering weapons, firing,
reloading, scopes, quick turns, pushing, stairs, damage, death, quick knife and
script/menu ownership block the action. A new press is needed after the block;
there is no queue. A request expires at the next input sample if the player
update did not consume it. Reload and other native actions starting in the same
tick take precedence. Simultaneous direct presses select the first ordinal;
opposite cycle directions cancel. Selecting the current weapon is a no-op,
avoiding the native inventory routine's toggle-to-unequipped behavior.

The native `WeaponSet` routine still updates the equipped index, ammunition-kind
weapon number and asynchronous model/motion loading state. The shortcut never
creates weapons, changes ammunition or overwrites another character's inventory.

Verification on the initial weapon-shortcut Windows x86 build:

- All 22 non-movie CTest checks passed, including 419 weapon inventory/state
  checks, real binding/INI round trips and the production input sampler with
  an SDL virtual gamepad. The sampler test covers overlay/focus loss, release
  before reuse, and background gamepad-only input. Nondefault primary binding
  round trips caught and verified the fix for the `weapon_2` / secondary `_2`
  name ambiguity. Nine parent source/tool/asset checks also passed.
- Both 2400-frame native opening runs passed: fixed camera and third person.
  Each selected handgun, magnum, knife, then cycled to handgun using remapped
  SDL controller inputs. Subsequent selections required native asynchronous
  loading to finish; the game remained playable and rendered the final handgun.
  Final ammunition stayed knife 0, handgun 7, magnum 15.
- Evidence is under `local/campaign-smoke/QuickSwitchFixedFinal` and
  `local/campaign-smoke/QuickSwitchThirdPersonFinal`; logs and screenshots are
  local verification artifacts, excluded from the package.
- The reproducible decomp patch applies to the pristine pinned source. The
  ZIP passed integrity checking, contains 15 allow-listed files and embeds the
  exact staged executable. Imports contain no Winsock library.

Initial executable: `build/campaign-platform/dist-quick-switch/AshfordsHalberd.exe`.
SHA256: `49813590485B211127213CD1B89241A6E92C9E5DABC3CF66B3A060672256DBD3`.

The state gates are regression-tested; physical controller feel and every
late-game animation have not been tested manually. These checks do not establish
a complete campaign playthrough. The movie decoder was unchanged and its earlier
full-disc validation was not repeated for this feature.

## Standard controller defaults follow-up

Hold left-stick click (PlayStation L3 / Xbox LS) and press D-pad Up, Right,
Down or Left for carried weapons 1, 2, 3 or 4. Hold the same modifier and use
LB/RB (PlayStation L1/R1) for previous/next. L3 alone does not switch. The base
input stays owned until released, even if L3 is released first, preventing a
follow-on knife, reload, quick turn or movement. Fresh navigation presses in
menus retain their normal behavior.

The final controller build passes all 22 non-movie CTest checks and nine parent
source/tool/asset checks. Tests exercise all six default combinations in both
schemes, their INI round trips and capture, old-default migration, explicit
unbinding, base-action suppression and release order. The 419 inventory/state
checks now include the native packed layout next to a two-cell weapon.

Both 2400-frame runs under `local/campaign-smoke/ControllerFixedFinal` and
`local/campaign-smoke/ControllerThirdPersonFinal` selected all four carried
weapons and cycled in both directions through the defaults, using an SDL virtual
standard controller. The native inventory sorted the rocket launcher to the
front; knife, handgun and magnum after it remained selectable. Final ammo was
rocket launcher 6, knife 0, handgun 7 and magnum 15. No custom controller remaps
were needed for these runs. Physical controller feel remains unverified.

Current executable: `build/campaign-platform/dist-controller/AshfordsHalberd.exe`.
SHA256: `3F54C8537EA3363C6A8653066FF4CDD7FF24CAC72E10A1B34ECD639C7DBF7885`.
