# Campaign first-person camera

Choose **Camera > Camera mode > First person** in F1 or OPTION > ENHANCED OPTIONS. Both menus expose the same settings. **F12** toggles first person; controller default is **hold L3/LS and press R3/RS**. Release the combination between toggles. Rebind the shortcut in Controls. F5 retains its third-person toggle; turning either optional camera off returns to Original.

First person shares the free-camera controls: WASD movement, mouse look, right mouse to aim, left mouse to fire, Shift to run, E to interact, R to reload and Tab for inventory. With modern controller controls, the left stick moves relative to the view and the right stick looks/aims; use the existing aiming, firing, interaction and inventory bindings. Camera-relative sideways and backward movement turns the underlying character to the movement direction while keeping the view independent. The game retains its native movement, collision and weapon rules, including stationary aiming.

Classic controller mode retains native tank controls and a character-facing view rather than twin-stick orbiting.

FOV, aiming FOV, sensitivity, dead zones, invert Y and crosshair options are shared with third person. Camera height adjusts eye height; shoulder offsets and camera distance do not move the first-person eye behind the character. Original and third-person choices and existing `tps = 0/1` configurations remain supported; `tps = 2` selects first person. `--first-person` starts with it selected.

The eye height follows the native Battle Game approach: local `(0, 1.5, -1)` at head node 5. Horizontal placement is stabilized, scaled for appearance changes and checked against walls. The PC camera layer keeps the room's normal camera cuts running for scripts, lighting and visibility. It does not enable Battle-only eye flags, which also change native movement and room processing.

Cutscenes, doors, puzzles, scripted interactions, stairs/climbs/pushing, damage/grabs and native weapon scopes temporarily use the original camera. First person resumes when suitable player control returns. Inventory screens keep their normal interface. The local player's body is hidden only from the active first-person main view; poses, collision, other actors and native script flags continue. This is a camera mode using existing weapon mechanics, without a separate first-person arm/weapon model set.

Full room snapshots preserve camera mode 2. As with every executable update, quickslots from previous builds are incompatible; native saves remain separate.

This optional mode needs broad campaign playtesting. Targeted opening tests do not establish that every late-game puzzle, boss, stair sequence or character appearance is correct.

## Verification

On 2026-10-25 the complete build passed 39 runnable tests; the disc-dependent movie test was skipped. Targeted tests cover head-height placement, aim-ray origin, camera collision, model availability, body-draw gating, script/scope fallback, movement translation, persisted mode and real SDL virtual-controller binding capture/release behavior.

An opening-campaign fixture exercised L3+R3 toggling, simultaneous controller movement/look, aiming, a handgun shot (7 to 6 rounds), inventory suspension/resumption, and full room quicksave/quickload while first person remained selected. This uses test-only equipment and input scripts; it is not a full campaign playthrough.

A second run used real F12 key events for off/on switching and repeated the movement, firing, inventory and snapshot checks on the packaged executable build. Its SHA-256 is `99c060b80cac924e61ef3dfcd8896ae98c0e881b7d5b42c9aa1ccb48ba68b3c9`.
