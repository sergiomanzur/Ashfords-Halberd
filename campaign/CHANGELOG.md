# Ashford's Halberd changes

## Unreleased — Android arm64 Foundation (2026-11-24)

Added the native `arm64-v8a` Android backend foundation alongside the existing Windows x86 campaign (zero Windows regressions):

- **Toolchain & on-device test runner:** Added `campaign/cmake/android-arm64.cmake` (NDK r26+, `arm64-v8a`, `minSdk 26`, 16 KiB max page size), `tools/build-campaign-android.ps1`, and `tools/run-android-ctest.py` (echoed exit-code verification over `adb` with explicit dispositions for Java-lifecycle-gated tests).
- **Windowed memory model (`librecvx_game.so` + `window_load`):** Linked the decomp and glue as `librecvx_game.so` with `campaign/cmake/android-window.ld` (data/bss in `[0x30000000, 0x40000000)`, code in `[0x40000000, 0x50000000)`), loaded via `MAP_FIXED_NOREPLACE` + `android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)` in `campaign/pc/src/host/window_load.{h,c}` with a 256 MiB 4-slot DMA host pool in `[0x50000000, 0x60000000)`.
- **64-bit memory/DMA/snapshot fixes:** Updated `memory.c`, `kernel.c`, and `dma.c` for Android segment auditing, DMA pool delegation, 64-bit INTC handler code-range validation, and the `RECVX_DMA_MAP_AUDIT` runtime trap; moved `ps2_vu0sub0`/`ps2_vu1sub0`/`ps2_vu1sub1` into `librecvx_game.so` (`vu_stubs_android.c`); fixed 64-bit `NJS_TEXNAME` offset punning in `sub1.c` `ItemSort()` via `offline.patch`; recorded the checked-in pointer inventory in `docs/android-pointer-audit.md`.
- **Clang 32-byte alignment parity & ELF64 snapshot manifest:** Added `ASHFORD_ALIGN32` to 14 EE/DMA game buffers via `offline.patch` and extended `snapshot_manifest.py` with `--so` ELF64 patching wired as an Android `POST_BUILD` step, verified on the Retroid Pocket 6 (`38` passed, `0` failed, `6` Java-lifecycle skipped). See `docs/android-backend.md`.

## 0.9.3b Beta (2026-11-24)

Complete offline single-player upstream sync through `recvx-enhanced-port` 0.8.0
(`8a1b6f066f9aab03618a428c575e034c2769da55`) and decomp `f30668f6fccddaa53ba3540b222a28fa09935165`,
delivered across three strictly single-player waves (zero online co-op, networking, auto-updater,
or online downloaders).

### Extra Item Boxes, Blue Herb Planters and Antarctica Typewriter

- Ported `Extra item boxes and herbs` (`Gameplay > Extra item boxes and herbs`), adding
  working item boxes, blue herb planters, and an Antarctica typewriter with custom save
  location strings (`SAVE_TEXT_ITM`) and full unit coverage (`test_placer`).
- Ported the `Ps2_PXLCONV` scratch buffer fix (`ps2_LoadTim2.c` / `ps2_dummy.c`) so dynamic
  texture uploads for extra models never corrupt room file reads.

### Offline BioRand support and In-Game Spoiler Log

- Ported strictly offline BioRand randomizer pack support (`mods/biorand/RM_*.RDX` and
  `log_pl0.txt`, zero online downloads or network code) with `Gameplay > BioRand randomizer
  pack` and `Rescan BioRand folder`, save-pack ID stamping & mismatch prompt, Claire item-box
  unblocking (`item.c`), cutscene cast shot audio fallback (`bhWeaponSeCall`), and BioRand
  seed codec + unit tests (`test_biorand_opts`, `test_spoiler`).
- Added the in-game BioRand Spoiler Log screen (default hotkey `L` via `B_SPOILER` or
  `SPOILER LOG` in Ashford Pause) browsing `log_pl0.txt` by area, segment, or key items with
  real TIM2 item icons and current-room jumping.

### Offline Resident Evil Outbreak File #2 Character Import

- Ported the offline Outbreak File #2 (SLUS-20984) disc importer (`src/import/import.c`,
  `impmodel.c`, `ob2.c`) with `.cvxm` CPU skinning, eyelid blinking, voice/cutscene jaw
  animation, two-handed grip alignment, Cutscene Cast integration (`Include imported models`),
  quicksave/quickload snapshot safety (`actor_snapshot_before_restore` / `after_restore`),
  and an interactive Field Dossier F1 importer (`Character > IMPORTED CHARACTERS`).

### Offline Languages, Cutscene Subtitles and Movie Overrides

- Ported offline multi-language disc extraction and runtime support (`lang_glue.c`, `glyphs.c`,
  `tr.c` + `TR_TABLE` resource, `src/import/lang.c`) supporting regional and fan-translated
  Code: Veronica X PS2 discs, independent cutscene subtitles (`Subtitles`), `Translated
  pictures`, `Translated title call`, Arabic RTL shaping, CJK wrapping, Japanese file-page
  pictures, memory-card text fitting (`pc_mes_fit_x`), US-plate preservation (`lang_us_plates`),
  snapshot restore safety (`lang_snapshot_after_restore`), Field Dossier F1 `LANGUAGE DISC
  IMPORT`, and unit tests (`test_import`, `test_lang`).
- Ported `mods/MOVIE/<name>.PSS` override support in `movie_glue.c` and `player.c` alongside
  checked AFS/mod file redirection in `adx.c` (preserving full audio snapshot fidelity).

### Enemies, sound banks and randomizer overhaul

- Expanded active enemy capacity from 32 to 255 slots with per-slot enemy sound banks,
  preserving full room-snapshot compatibility (`.ahsnap`).
- Overhauled the enemy randomizer with distinct Hunter and Sweeper types, `Types per room`
  variety control, and custom per-type spawn weight sliders (`rate_list.c` in native OPTION
  and `enemy_rates_imgui` in Field Dossier F1).
- Upgraded `Extra enemies` (up to 8 per room) and `Rearranged enemies` with ground-height
  validation, door-margin spacing, and persistent kill tracking across room visits.

### Appearance, animations and cutscene fixes

- Added per-character model selection (`Character models: Same for both / Per character`
  with separate `Claire's model` and `Chris's model`, plus `Current Character (Cutscene)`).
- Fixed the 9100 Spotter cutscene softlock (`event.c`), added double-node shoulder retargeting,
  ankle/toe grounding, spinning-hand and unarmed-hand fixes, gender-matched leg sync for
  Move While Aiming, and Wesker's afterimage trail (`trail_glue.c`).

### Cutscene Cast and Voice-Line randomizers

- Ported the offline Cutscene Cast randomizer (`Randomize cutscene cast`, `Cast model pool`,
  `Cast assignment`, `Randomize cutscene monsters`, `Cutscene enemy models`) with custom model
  and monster checklists (`cast_list.c` + Field Dossier F1).
- Ported the deterministic Cutscene Voice-Line randomizer (`Randomize voice lines`, `Talking
  mouths`) with background disc voice cataloguing + cache, ADX/synthesizer envelope-driven
  lip sync, and unit coverage (`test_cast`).

### Menus, visual item picker and UX polish

- Added an Ashford-styled in-game Pause Menu (`Gameplay > Pause menu: Ashford Pause / Classic
  OPTION`) with one-tap Camera cycling across Original, Third person, and First person,
  active campaign modifiers summary, hotkey card, and direct access to Enhanced Options,
  Appearance & Cast, Cheats & Tools, Classic OPTION, and Return to Title.
- Added a Title Screen `EXIT` row with a confirmation dialog (`Quit the game?`).
- Added a Visual Give Item picker with real `ITEM AFS` inventory icons, categories, quantity
  stepping, and item-box overflow fallback in both native OPTION (`give_list.c`) and Field
  Dossier F1 (`Cheats > ITEMS`).
- Added procedural vector controller/keyboard button glyphs (`Controls > Button icons`),
  10-second Keep-or-Revert confirmation for Display Mode and Fullscreen Resolution changes,
  Restart and Next-Room badges, and a live Performance Graph HUD (`Display > Frame rate HUD:
  Off / FPS / FPS + Graph`).

### Renderer, anti-aliasing and performance

- Completed 3D-only FXAA and SMAA (`gs_scene_aa`) keeping 2D menus, fonts and HUD sharp,
  and added 16-bit floating-point render targets with IGN dithering and optional Deband
  (`Dithering: Off / On / Deband`) while preserving Ashford's Saturation control.
- Ported the 0.7.2/0.7.7 GS and interpolation optimizations: `TEX0` batch merging,
  specialized GS fragment shader variants, block-by-block GS texture decode and CLUT loads
  (including the GCC `-m32 -O3` `PSMT8`/`Z16` `gs_read_rect` fix), capture pool reuse and
  GPU clears, bbox-culled interpolation matching, underwater smooth motion, slow-GPU
  present fallback, and ini-only `dyn_res` support.

### HUD, audio and engine fixes

- Completed the Ashford HUD (Weapon Circle and Minimal layouts) with real spare-ammo
  counting matching `bhSearchBullet` (including side pack and gas rounds), ammo-type
  coloring, TIM2 weapon icons extracted offline from `ITEM AFS` file 145, and fixed
  shoulder-follow placement for first-person and fixed cameras.
- Added `snd_sysbank_load()` fallback so F2 Save Now / Load Game / Open Item Box play
  menu sound effects outside save rooms.
- Ported small engine/decomp fixes: widescreen B.O.W. gas effect framing (`bhDrawNtxEffect2D`),
  Smooth Menus arrow-sliding fix, unnamed-room `Type_Space[-1]` bounds guard for F2 Save Now,
  and ISO9660 directory record length validation.

### Fixed-camera controls and input polish

- Added Combined fixed-camera controls (`Fixed camera controls: Combined (d-pad tank)`,
  default on first start and in the Enhanced preset, with `fixed_cam_version` migration).
- Added rebindable fixed-camera `Aim (no auto-aim)` (`E` / Left Trigger, `bindings_version = 4`
  migration preserving existing controller weapon shortcuts).
- Enabled mouse buttons on fixed-camera game bindings, OPTION Enter confirmation, trigger/stick
  release waiting after binding capture, 2-frame item-box wheel scrolling, and single-owner
  thread-safe gamepad metadata (`bind_pad_desc`).

### Tests

- Added `gsmem` (`-O3`), `aa_tables` (SMAA LUT hashes), and offline `parsers` unit tests,
  plus dithering shader, Combined/Free Aim, and OPTION Enter regression tests.

## 0.9.2b Beta – 2026-11-15

### Upstream sync (single-player subset of recvx-enhanced-port 0.6.9–0.8.0)

- Added always-on `AshfordsHalberd-crash.log` reports with the last messages and a Windows
  frame-chain backtrace, independently of `--log`.
- Added checked PRS/AFS/DDS/PNG parsing and bounded HD-pack loading; damaged mod files and
  oversized replacements are skipped instead of crashing.
- Added startup memory diagnostics that name the program holding the PS2 scratchpad address.
- Enabled `-O3` for the GS/render hot paths.
- Ported the SMG reload, lighter/lockpick item-box, albinoid/Monster Steve hold, scripted-stair,
  model-less enemy, weapon 0/1 damage, mod-room leave, Battle Game exit and texture-scratch fixes.
- Added third-person locational hit reactions, headshots/piercing, move-while-aiming and
  room-model camera collision.

### Gameplay and quality of life

- Added rifle/assault-rifle ammunition stacking (Gameplay > Rifle ammo stacks).
- Added memory-card slot 2 (`card1`) and 30-slot memory-card import.
- Added an Ashford-styled HUD (Auto/On/Off, shoulder side, Weapon Circle/Minimal).
- Added Save Now, Load Game, Open Item Box and Remove Item to the shared Cheats page.
- Added SMAA and Dithering (Off/On/Deband) to Display.
- Kept the existing launcher, Field Dossier, first-person camera, quicksave and weapon shortcuts
  and their look and feel.

### Compatibility and limits

- Native saves remain compatible. Quicksave snapshots are tied to the exact executable build;
  use a native save when upgrading.
- BioRand, disc imports, languages, the new upstream menus and co-op remain outside this
  offline build.
- The world-placed "Extra Item Boxes and Herbs" objects are not included in this release.
- Full campaign playthrough coverage remains outstanding.

## 0.9.1b Beta – 2026-11-02

- Fixed a persistent black screen after skipping the first-zombie graveyard FMV.
  Gameplay continued behind a black fade forced by the PC skip override. Movie
  skipping now respects the original per-movie and caller-requested fade policy.
- Kept F8 skipping available for every movie and preserved native fades for
  scenes and endings that require them. The fix applies to every camera mode.
- Added production-code movie lifecycle regressions and a real graveyard FMV
  return check with first person and modern controls.

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
