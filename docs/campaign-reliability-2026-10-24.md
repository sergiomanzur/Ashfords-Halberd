# Campaign reliability: reload, controls and presentation

This update investigates upstream reports #62, #60 and #41 against the offline
Ashford campaign. It does not restore any networking functionality.

## Reload (#62)

The real `bhSearchBullet` routine checked the inventory-expansion bit in
`sys->st_flg`. Inventory allocation, acquisition and other inventory operations
use `sys->gm_flg`. The wrong flag excluded the last two expanded slots, or could
inspect locked slots when an unrelated status bit happened to be set.

The fix uses the inventory flag. There is no adjacency workaround. Ordinary
non-adjacent ammunition in valid eight-slot inventories passed the matrix
before the fix; the broader adjacency claim was not reproduced there.

The regression executable selects the complete production ItemSort, Combi_99,
bhSearchBullet and bhCheckBullet routines, real weapon definitions and retail
item/combine tables. It checks 42,720 combinations across seven reloadable
weapon/ammunition pairs, legal weapon/ammo positions, eight/ten-slot capacity,
four game modes, partial/empty magazines, and automatic/raised/lowered reload
paths. It also checks locked slots, incompatible ammunition and infinite reserve.
Reserve conservation and the equipped weapon are checked after sorting.

The initial 28,480-case matrix produced 5,233 failed assertions with the pinned
original routine and zero with the correction. The expanded 42,720-case matrix
passes. Two-slot weapons use valid packed inventory layouts.

## Input, interactions and vibration (#60)

- Modern fixed-camera movement previously rewrote directions even when an
  event/puzzle paused player simulation. It now respects the player simulation
  bit and scripted movement ownership. It rechecks ownership after the native
  mode handler, so an interaction starting in that handler retains facing.
- Third-person input has the same ownership protections. Native puzzle arrows
  are not transformed into camera-relative walking; the gamepad returns to
  native bindings while the player is paused. Third-person facing changes stop
  when a scripted interaction owns the player.
- Contact with an actual movable object suppresses run in the movement hooks,
  allowing the original walking-to-pushing state transition. Running remains
  available away from push contact. Original fixed-camera tank input is retained.
- The native OPTION pointer handler now ignores the VIBRATION row when there
  is no compatible actuator, matching the native menu. Previously pointer
  selection and the native disabled-row correction could fight every frame,
  repeatedly playing the selection sound.

Tests exercise production modern/TPS pad conversion and option-pointer code.
They cover paused-puzzle left/right input, ownership changes inside the mode
handler, normal movement, run/push contact, staircase camera rebasing, and 100
pointer updates over an unavailable vibration row. SDL virtual gamepads test
rumble capability detection, both motors at 0/50/100%, background suppression,
and the unused controller port. Physical motor feel is not established by these
virtual-device checks.

The exact late-campaign cube puzzle, palace/moth-hall staircases and item-box
scene have not been played through in this update. Their underlying control
states are exercised by the regression harnesses; scene-specific acceptance
still needs in-game saves or progression to those locations.

## Presentation/frame pacing (#41)

Two scheduling faults were corrected:

1. A smoothed history of blocking swaps could keep software pacing disabled
   after the current swap started returning immediately. The classifier now
   requires both current and smoothed evidence that the swap is pacing.
2. The render timestamp was captured before a GPU-fence wait. At 60 Hz, a
   successful 14 ms wait could leave only 2.667 ms until the next old-grid
   deadline. The scheduler now samples time after that wait, updates timed
   interpolation, and rebases long delays instead of creating a catch-up burst.
   Small timing deviations retain the existing grid.

Both behaviors have failing-before/passing-after deterministic regressions.
The presentation trace now records completion after rendering and buffer swap,
alongside start time and swap-block duration, plus the driver's reported interval.

Measurements used an RTX 3080, driver 32.0.16.1088, 60 Hz desktop at 3840x2160.
Each run executes 900 game frames with the same RM_0000 opening script; frames
before 300 are excluded. Internal resolution stays at 2x. Windowed output is
requested at 1280x720; borderless and exclusive select the desktop mode. Mode comparisons thus
include output-size/compositor differences. These are CPU presentation-completion
intervals, not external measurements of physical scanout.

| Display mode | VSync | p95 ms | p99 ms | Maximum ms |
|---|---|---:|---:|---:|
| Windowed | On | 17.13 | 17.82 | 18.80 |
| Borderless | On | 17.25 | 18.89 | 22.79 |
| Exclusive | On | 17.23 | 18.28 | 20.08 |
| Windowed | Off | 17.56 | 18.31 | 19.36 |
| Borderless | Off | 17.45 | 18.07 | 19.35 |
| Exclusive | Off | 17.68 | 18.62 | 20.86 |

Across 7,208 measured intervals the mean is approximately 16.667 ms in every
configuration, with no intervals above 25 ms or below half a refresh. Raw
summaries: [CSV](benchmarks/2026-10-24-presentation-times.csv).

The prior executable was measured first in all six configurations. An additional
unchanged-scheduler control with completion timestamps measured VSync-on p99s
of 17.80/16.79/17.99 ms for windowed/borderless/exclusive. The driver's blocking
behavior varied across runs. These results do not demonstrate a blanket speed
improvement or establish that the original reported water-camera stutter is
fixed on every GPU. A 1.308-second stall in the initial exclusive baseline did
not recur in the control/final runs and has no confirmed root cause.

## Reproduce

Run `tools/measure-campaign-pacing.py` with `--executable`, `--iso`, and `--output`.
It creates isolated configurations, runtime logs, logic profiles and presentation
traces; it does not modify player saves. Runtime traces stay under `local/`.

The measured executable SHA256 is
`10613c66c236fc9138c5e3a173696bfda8b4ac235114cfd74a50c03b7bad6bd4`.
The source adaptation is preserved in `campaign/patches/offline.patch` and its
fresh-checkout application is checked separately.

## Verification of this build

Twenty non-movie component checks pass, including the reload matrix, native
input/option handlers, virtual-device rumble and pacing regressions. Nine
source/tool/asset checks pass. Both 2400-frame opening gameplay runs pass:
fixed-camera movement records 180 distinct positions, third-person 304; both
open and close the original inventory. Screenshots were inspected.

Full-disc movie decoding was verified in the preceding baseline and was not
rerun for this update; the decoder is unchanged. The new decomp patch passes
both clean-checkout application and already-applied checks. Full campaign
completion remains outside this update's verification.
