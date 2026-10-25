# Ashford's Halberd changes

## 0.9.0b Beta — 2026-10-25

### Campaign and identity

- Integrated the native campaign backend with original-disc validation and an
  offline, single-player build. Requires your own unmodified USA PS2 ISO.
- Removed online sessions, co-op, chat, trading, servers and network dependencies.
- Added original launcher artwork, a halberd icon and the ambient launcher score
  The Quiet Threshold, with disc, display and volume controls.
- Replaced the F1 presentation with the Field Dossier. F1 and OPTION > ENHANCED
  OPTIONS share video, audio, gameplay, camera, appearance, binding and cheat settings.

### Camera, saving and controls

- Added optional campaign first person using Battle Game's head-node eye-height
  approach through the PC camera layer. F12 or L3/LS + R3/RS toggles it. Includes
  mouse/controller look, camera-relative movement and scripted-camera fallback.
- Added full room quicksave/quickload: F9/F10, or L3/LS + Back/View and Start/Menu.
  Captures room objects, local flags, event tasks, graphics and logical audio state
  instead of reopening the room. Preserves native saves and save/retry counters.
- Added rebindable carried-weapon cycling and four direct slots, including
  controller L3/LS + LB/RB and D-pad Up/Right/Down/Left. Uses carried weapons only,
  preserves ammo and blocks incompatible animations or scripted actions.
- Added F8 FMV skipping through native fade and cleanup.
- Added shared Cheats pages, controller combination capture, input ownership
  through release and protection against delayed actions after blocked input.

### Picture, audio and reliability

- Added saturation, optional FXAA and Original / Enhanced / Custom presets while
  keeping individual settings accessible in both menus.
- Retained internal-resolution, aspect/FOV, display-rate interpolation, picture
  calibration, fullscreen, VSync, fog/effects and separate audio-volume options.
- Corrected expanded-inventory reload lookup, puzzle/interaction input ownership,
  running-to-pushing behavior, unavailable vibration-row selection and pacing edges.
- Added regression tests for input, inventory, graphics, audio, snapshot integrity,
  build compatibility and original-camera fallback.
- Investigated Alfred's stairway visibility: the original script explicitly hides
  both actor records. No speculative visibility or actor behavior change was added.

### Beta limits and compatibility

- Complete campaign playthrough coverage remains outstanding; late-game scenes,
  bosses and first-person interactions still need playtesting.
- First person uses existing weapon mechanics without a separate arm/weapon
  model set. Scripts, doors, puzzles, incompatible movement, damage/grabs and native
  scopes use their original cameras.
- Quick actions require safe player control; active enemies, countdowns, scripts
  and incompatible animations block them. Each quickslot requires the exact
  executable build. Previous-build quickslots are incompatible; keep native saves
  for progress across updates. Audio device-buffer recreation may cause a brief edge.
- The earlier x64 prototype remains experimental; its original-zombie playable-slice
  acceptance test is a documented failure. It is separate from the campaign build.
- No retail assets, ISO, SDK headers or saves are included in the release package.

## Picture controls and presets — 2026-10-24

- Added shared 0–200% saturation, neutral at 100%, and included it in picture reset.
- Added optional final-image FXAA, independently of internal resolution and off
  by default. Crop-clamped sampling protects display boundaries and black bars.
- Added Original / Enhanced / Custom experience presets to both menu frontends,
  with all individual settings retained. Preserve bindings, saves, audio, camera
  tuning, renderer choices and cheats. Existing settings start as Custom.
- Added native GPU shader checks, profile preservation tests and runtime
  verification of shared settings transitions and persistence.

## Controller weapon defaults — 2026-10-24

- Assigned all six shortcuts on standard controllers: L3/LS + D-pad
  Up/Right/Down/Left selects weapons 1/2/3/4; L3/LS + LB/RB cycles backward/forward.
- Added controller-combination labels, capture and saved bindings in both editors.
- Preserve normal D-pad navigation without the modifier. Consume the base button
  until release to prevent a follow-on quick knife, reload or quick turn.
- Upgrade the preceding default-only controller assignments automatically while
  retaining custom bindings and explicit unbinding in the new format.
- Corrected packed inventory scanning: a weapon after a two-cell item remains
  selectable. Two-cell items use one stored inventory word, not a second entry.

## Weapon shortcuts — 2026-10-24

- Added six shared, rebindable keyboard/mouse/gamepad actions: previous/next
  carried weapon and direct carried weapons 1–4, in both binding menus.
- Defaults: bracket keys cycle, F3/F4/F6/F7 select, left-stick click cycles next.
- Select from the current character's carried grid only, including expanded
  inventory; two-slot weapons count once and empty magazines remain usable.
- Use native equipment/model loading and preserve ammo. Reject presses during
  scripts, menus, damage, aiming/attacks/reloads, pushing and quick-knife ownership.
- Sample edges during pauses and recheck after player actions to avoid deferred
  switches. Added inventory/state and binding regressions and a runtime smoke mode.

## Offline identity update — 2026-10-23

- Removed online sessions, co-op, lobby/chat/trade, server transport and router
  mapping code, dependencies, configuration entries and menus.
- Preserved single-player appearance swapping and native saves independently.
- Unified enhancement preferences and binding editors in the Field Dossier and
  the game's Enhanced Options menu. Added the shared Cheats page and F2 shortcut.
- Added native timer/ranking tools and safe stacked/two-slot item grants.
- Added original halberd icon, manor launcher illustration and an original
  synthesized ambient composition, The Quiet Threshold.
- Added a native same-executable launcher with ISO selection, display and music
  controls, disc validation recovery and independent automated test hooks.
- Added reproducible decomp adaptation, original asset provenance and offline
  package/source checks. Rewrote the parent README around actual game status.

The imported engine baseline is recvx-enhanced-port 0.6.8; source attribution
and the exact pins are recorded in PROVENANCE.md. Current validation and limits
are documented in the parent README and docs/campaign-backend.md.

- Added rebindable F8 FMV skipping through the normal picture/audio fade, with stale presses cleared at each movie start.


## Reliability update — 2026-10-24

- Fixed the expansion flag used by native ammunition lookup; added an exhaustive
  real-source reload matrix for all legal inventory positions and reload paths.
- Preserved puzzle directions and scripted facing in both modern camera schemes;
  allowed the native push transition while run is held at a movable object.
- Stopped the native VIBRATION pointer row from fighting its unavailable state.
- Added virtual-device motor/capability and staircase/camera state regressions.
- Corrected stale GPU-wait timestamps and software-pacing catch-up behavior;
  recorded windowed/borderless/exclusive results with VSync on and off.
- Detailed scope, measurements and unverified scenes are in the parent report
  docs/campaign-reliability-2026-10-24.md.
