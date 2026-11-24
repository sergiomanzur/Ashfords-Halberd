# Ashford's Halberd Android port

The user directed a port of the playable campaign to Android with 1:1 PC
parity, working video settings, toggleable touch controls, and automatic
update checks on both the Windows app and the Android APK. The user picked
the update channel (GitHub Releases) and the Android update behavior
(automatic check on every open + one-tap install prompt), and connected a
Retroid Pocket 6 over adb for device verification. Technical decisions are
delegated; implementation proceeds in this chat under swarm mode.

## Intent and why

The campaign backend (SDL3 + OpenGL 3.3 core, Windows x86) should run as a
native Android app on modern 64-bit-only devices, with the same game logic,
enhancements, settings and update convenience as the PC build. Success is:
the game boots and plays on the named devices from the user's own ISO, every
video setting functions, touch controls are available and toggleable from
both menus, and both apps keep themselves current from GitHub Releases.

## Goals

- Native arm64-v8a Android app from the existing campaign source. No
  emulation layer, no separate code fork: one source tree, two platforms.
- 1:1 parity with the PC campaign: same decomp game code, same offline
  patch, same settings table, same bindings system, same saves/settings
  formats (the `recvx.ini` and native save formats remain identical).
- All video settings function on Android: internal resolution scale +
  dynamic resolution, FXAA/SMAA, dithering (Off/On/Deband), saturation,
  brightness/contrast/gamma, VSync, FPS limit, aspect/FOV, AA, presets.
- Touch controls: virtual gamepad overlay (analog stick + D-pad option, DS2
  face/shoulder buttons, drag-look), shown/hidden from the F1 Field
  Dossier, the native OPTION > ENHANCED OPTIONS menu, and the Ashford
  Pause overlay; persisted; auto-hides when a physical gamepad connects.
- Update checks on every app open for both platforms against GitHub
  Releases. Windows: download, apply, restart automatically. Android:
  automatic check, one-tap install prompt (the OS install confirmation
  itself cannot be bypassed without root; the user accepted this).
- Device targets: Samsung S24 Ultra (Adreno 750, 64-bit-only), Retroid
  Pocket 6 (measured: Android 13, QCS8550/SD8Gen2, Adreno 740, GLES 3.2,
  1080x1920, connected via adb), and Mali-G-series GPUs (GLES 3.2 class)
  via capability detection and documented fallbacks.
- The Windows x86 build remains the canonical build and is not regressed.

## Non-goals

- No Play Store / F-Droid distribution (Capcom IP fan project; GitHub only).
- No Vulkan renderer, no NEON VU assembly (the portable plain-C VU path is
  the Android path; revisit later).
- No armeabi-v7a, x86_64 or iOS targets. The named devices are 64-bit-only.
- No Android-exclusive gameplay features, and no touch support on PC.
- No new gameplay networking. The offline identity is preserved; only the
  narrow update-check module contacts the network.

## Anti-goals

- Regressing the Windows x86 campaign build or its 50/50 test suite.
- A second, divergent game codebase for Android.
- Shipping retail game data in the APK.
- Weakening the offline-identity contract silently: adding the updater must
  come with an updated, still-strict contract test.

## Measured facts (spike, 2026-11-24, Retroid Pocket 6)

- `mmap(MAP_FIXED_NOREPLACE)` at `0x30000000` (256 MB), `0x10000000`
  (256 MB) and `0x70000000` (16 MB) all succeed and are writable; a plain
  address hint at `0x30000000` is honored; `mmap_min_addr` is 32 KB.
  **The identity-mapped PS2 memory window is viable on arm64 Android.**
  Probe: `local/spikes/android-mmap-probe/probe.c` (throwaway, untracked).
- OpenGL ES 3.2 (Adreno 740). S24 Ultra (Adreno 750) and recent Mali
  G-series also support GLES 3.2 + `EXT_color_buffer_float`.

## Approaches considered

1. **Native arm64 recompile + identity memory window + GLES 3.2 backend**
   (chosen). Full parity, full performance, one source tree. Cost: memory-
   model work for 64-bit, a GL ES shader/profile variant, NDK packaging.
2. **32-bit armeabi-v7a build** (rejected). Keeps the memory model but
   cannot run at all on S24 Ultra or Retroid Pocket 6 — both 64-bit-only —
   so it fails the user's named devices outright.
3. **Emulation container** (Winlator/Box64-style x86 emulation, or a GS-level
   wrapper) (rejected). Performance cost on the hot GS/DMA paths, loss of
   parity control, and a heavy third-party stack around MIT source we
   already compile natively.

Updater channel: GitHub Releases (chosen — releases already live there) vs.
a store channel (rejected: IP takedown risk, and silent sideloaded updates
are impossible on Android regardless).

## Design

### Memory model (the core 64-bit change)

The decomp's 32-bit contract splits into two pointer populations:

- **EE-space identities** — pointers into PS2 memory (disc data pointers,
  scratchpad, uncached aliases). Keep the existing identity trick: mmap the
  PS2 regions at their literal addresses with `MAP_FIXED_NOREPLACE`
  (measured viable). A startup check verifies each mapping landed at its
  requested address and fails loudly with a clear message if a future
  device/kernel refuses.
- **Host-space pointers stored in 32-bit fields** (DMA tags, packet
  builders, snapshot regions; today valid because the x86 exe is linked at
  image base `0x30000000`). On Android every binary is PIE and ASLR is
  mandatory, so the exe cannot be linked low. Instead: a dedicated **low
  host pool**, mmap'd at `0x10000000` (measured viable), owned by
  `ps2/memory.c`, from which every host allocation whose address is stored
  into a 32-bit field is allocated.

Implementation shape:

- A one-time **pointer-field inventory** of every 32-bit field that receives
  a host pointer (`pc/src/ps2/dma.c`, `gs/stream.c`, glue files, snapshot
  code). Each site either switches its allocation to the low pool, or is
  proven EE-space-only. The inventory is recorded as a checked-in audit
  document with per-site evidence, plus a runtime debug-mode trap that
  verifies no out-of-window pointer enters a 32-bit DMA field.
- Lift the `CMAKE_SIZEOF_VOID_P == 4` fatal guard behind a new
  `ASHFORD_ANDROID`/windowed-memory mode; the 32-bit guard stays for all
  other configurations.
- Clang layout parity: replace the reliance on GCC's 32-byte array
  alignment with explicit alignment attributes (via `pc_prelude.h`, which is
  force-included into every decomp TU) plus compile-time offset asserts on
  the affected packet/EE buffers, so the NDK's required Clang toolchain
  produces the same layout GCC does.
- If the inventory finds decomp statics whose addresses leak into 32-bit
  fields, migrate those specific objects into the low pool through the
  established `#ifdef TARGET_PC` patch mechanism (`offline.patch` pattern).

### Renderer

GLES 3.2 backend in the same `gs_gl.c`/`picture_shader.h` code paths:

- `gl_loader` gains an ES profile mode; window/context via SDL3
  (`SDL_GL_CONTEXT_PROFILE_ES`, 3.2). All entry points still resolve through
  `SDL_GL_GetProcAddress`.
- Shaders: one source, two targets — `#version 330 core` (PC) vs
  `#version 320 es` with precision qualifiers (Android). No Vulkan, no
  ANGLE dependency.
- RGBA16F render targets require `EXT_color_buffer_float` (present on
  Adreno 740/750 and modern Mali). Where absent, fall back to RGBA8 with
  the existing dither approximation and mark the running mode in the
  Performance Graph HUD and settings help. All video settings keep
  functioning; float-target mode is the capability-gated part.
- ImGui's OpenGL3 backend runs GLES natively (`#version 300 es`); the
  Field Dossier, Cheats page and Ashford Pause render unchanged.

### Window and video settings mapping

- Android is always fullscreen: `fullscreen` toggles immersive mode vs
  letterboxed presentation; the PC window-size presets become output
  letterbox choices (4:3 / widescreen / stretch / integer-scale); exclusive
  display-mode selection maps to the surface refresh mode where available,
  otherwise the row reports the active mode and is inert (stated in its
  help text, not hidden — parity means present and honest).
- `vsync`, `fps_limit`, `scale` (+ `dyn_res`), FXAA/SMAA, dithering/deband,
  saturation/brightness/contrast/gamma, aspect/FOV, presets: direct ports.
- Native resolution default `scale=0` (auto) adapts to the device; the
  S24 Ultra's 1440p-class panel and the RP6's 1080p panel are both handled
  by the existing auto path.

### Input and touch controls

- Touch events (SDL3) feed a virtual gamepad overlay drawn with ImGui in the
  same style layer as the Field Dossier: left analog zone (with D-pad skin
  for fixed-camera play), DS2 face buttons, shoulders, Start/Select
  equivalents, and right-half drag-look for third-person/first-person
  aiming. Multi-touch chords supported.
- The overlay synthesizes input through the existing paths: DS2 pad
  reports in `pc/src/input/pad.c` and synthetic pointer-look for
  `tps_input.c` — so the rebindable bindings system, camera modes, weapon
  shortcuts, quicksave chords and first person all work unchanged.
- Show/hide toggles: one shared settings row (the existing single-settings-
  table pattern) exposed in F1 Field Dossier, native OPTION > ENHANCED
  OPTIONS, and Ashford Pause. Auto-show when no gamepad is connected,
  auto-hide when one is (overrideable). Opacity and scale sliders included.
- Retroid Pocket 6's built-in controls are already an SDL3 gamepad and work
  through the existing bindings with no new code; touch hides itself there.
- Scripted touch automation (a `RECVX_VPAD` analogue for touch) so the
  smoke harness can drive and verify touch UI headlessly.

### Updater (both platforms, one narrow module)

- `pc/src/host/update_check.c` (+ small platform glue): GET
  `api.github.com/repos/sergiomanzur/Ashfords-Halberd/releases` with a short
  timeout, pick the highest-version tagged release (betas included — they
  are the project's release line), compare against `ASHFORD_VERSION`.
  Failures never block play; the game stays fully playable offline.
- **Windows**: check on every launcher open. If newer: download the
  `AshfordsHalberd-Campaign-Windows-x86.zip` asset, verify the asset
  digest from the release API, extract to a staged directory, swap the
  install (the launcher performs this before spawning the game so no exe is
  running), and restart into the new version automatically.
- **Android**: check on every app open (launcher/main menu). If newer APK
  asset: inline prompt `Install update 0.9.x?` → one tap → download with
  progress → package-installer intent. Requires the
  `REQUEST_INSTALL_PACKAGES` permission; the user grants "install unknown
  apps" for the app once. Version comes from `campaign/cmake/version.cmake`
  as the single source, wired into Gradle `versionName`/`versionCode`.
- **Offline-identity contract**: `tests/offline_identity_tests.py` is
  amended to permit exactly the update-check module (own file scope, no
  game-thread network use, no sockets in game code, no lobby/co-op/trade
  glue) and still forbids everything else. Parity of the offline game
  itself is unchanged.

### Packaging and build

- New `campaign/android/` Gradle app: `minSdk 26`, `arm64-v8a` only, CMake
  external project invoking the existing `ASHFORD_CAMPAIGN` build with a
  new `campaign/cmake/android-arm64.cmake` toolchain file (NDK Clang,
  PIE). SDL3 `release-3.4.16` (same pin as PC) builds for Android from the
  same FetchContent; ImGui/xxHash/bcdec unchanged.
- `tools/build-campaign-android.ps1`: NDK discovery, Gradle assemble +
  APK, ctest (host component suite), packaging step that attaches the APK
  as a GitHub release asset alongside the Windows ZIP.
- Retail ISO: never shipped. First-run picker uses the system file picker
  (SAF/JNI) to import the user's SLUS-20184 ISO into app-private storage
  once, remembered thereafter; parity with the PC disc picker flow.
- Launcher branding assets ride in the APK `assets/`; SDL3 base path
  resolves them the same way PC does.

### Performance

- Native arm64 game + render threads, same threading topology as PC.
- VU microcode runs the existing portable plain-C path (`RECVX_VU_PORTABLE`
  was already written and tested for an arm64 build).
- `scale=0` auto + `dyn_res` adapt to device load; the existing Performance
  Graph HUD measures on-device.
- adb-driven benchmark smoke on RP6 and S24 Ultra records frame times for
  the opening run; performance parity target: 60 fps sustained on RP6 at
  auto scale with the Enhanced preset minus float targets if needed.

### Testing and acceptance

- Host portable ctests keep passing unchanged (Windows x86 suite is the
  regression gate).
- Portable test binaries (`vu_portable`, `gsmem`, `bindings`, `parsers`,
  `presets`, ...) cross-compile for arm64 and run on the RP6 via an
  adb-driven ctest wrapper.
- Frame-dump golden comparison: the existing `campaign-smoke` harness gains
  an Android mode — `RECVX_DUMP_FRAMES` on device, pulled via adb, compared
  against PC goldens for the opening scene across all video settings.
- Touch UI verification: scripted show/hide from all three menus, gameplay
  via injected touch, gamepad-connect auto-hide.
- Updater verification: staged local release fixture (no live GitHub in
  tests) covering check, compare, download, swap/restart on Windows and
  prompt→install intent on Android.
- Acceptance: the app installs on RP6 and S24 Ultra, boots from the user's
  ISO, New Game opening 2400-frame run via touch and via built-in gamepad,
  settings changes persist, all video settings visibly function, saves
  round-trip, and both updaters update from a fixture release. Full campaign
  completion remains a playthrough claim beyond automated gates, same as
  PC.

## Calls made by the architect (correct or veto)

- GLES 3.2 (not Vulkan, not ANGLE) as the Android GL path.
- arm64-v8a only; `minSdk 26`.
- Windows auto-update applies without asking (launcher-era swap), with the
  update reported in the UI; Android asks with one tap because the OS
  forces a confirmation anyway.
- ISO imported once via SAF into app-private storage (no persistent SAF
  permission juggling, no external-storage requirement).
- Touch overlay rendered with ImGui in the shared style layer.
- The offline-identity test contract is amended rather than abandoned.
- Update-check code lives in its own narrow module and is the only
  networking in the project.

## Left to the builder

- The pointer-field inventory itself (mechanism specified above; the sites
  and their dispositions are findings to produce and record).
- Exact GLES capability fallback thresholds (which devices get float
  targets) tuned against the device matrix as evidence accumulates.
- Touch layout details (button placement/size defaults) within the described
  shape, plus the touch automation hook design.
- Gradle/NDK integration specifics for SDL3 FetchContent on Android.
- Mali on-device verification is capability-based until a Mali device is
  available; the design does not block on owning one.
