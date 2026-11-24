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
  (256 MB), `0x50000000` (64 MB) and `0x70000000` (16 MB) all succeed and
  are writable; a plain address hint at `0x30000000` is honored;
  `mmap_min_addr` is 32 KB.
  **The identity-mapped game window at `0x30000000` and the host pool at
  `0x50000000` are both viable on arm64 Android.**
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

On PC the identity trick is total: the whole executable is linked at image
base `0x30000000`, and **all game working memory is the image's own
`.data`/`.bss`** — the room arena, texture heap, spawn pool, the entire
256 MB window (memory.c asserts statics end below `0x40000000`, and even
contains a runtime probe that a platform static carries the `0x30000000`
bit). Disc pointers are relocated in place into that window, `dma.c`
stores window/heap addresses into 32-bit DMA tags, and the quicksave
manifest enumerates statics by fixed address from the post-link image.
This is not "a few host allocations"; the decomp's statics are dual-natured
by construction.

Android binaries are PIE with mandatory ASLR, so the image cannot be
linked low. The mechanism is therefore:

- **Game data window**: keep the identity layout by moving the game's
  data/bss *image* into the mmap'd window at `0x30000000` (measured
  viable on the RP6). Build the decomp + platform game state as a
  separate linked data image with a build-generated relocation table;
  at startup a small loader maps the window, copies the data image in,
  and applies its relocations against the window base. Code stays in the
  PIE at high addresses — only data needs window identity. This
  generalizes the existing build-time manifest tooling (the PE-specific
  `snapshot_manifest.py` gains an ELF/arm64 mode) rather than
  mass-rewriting statics.
- **Host pool**: host allocations whose addresses enter 32-bit fields
  (DMA chain buffers, packet builders, snapshot regions) come from a
  dedicated pool mmap'd at `0x50000000` (measured viable) — above the
  game window, below the scratchpad, inside 31 bits. This gives one
  coherent Android address map:
  `[0x30000000, 0x40000000)` game window / PS2 identities (unchanged
  semantics); `[0x50000000, 0x70000000)` host pool (today the x86 heap
  happens to sit there by link luck; Android allocates it explicitly);
  `0x70000000+` scratchpad; everything else high and never stored in
  32-bit fields. `ps2_dma_ptr` and `memory.c`'s allocation guards are
  updated to encode exactly this map, so misclassification cannot pass
  silently.

Implementation shape:

- A one-time **pointer-field inventory** of every 32-bit field receiving
  a pointer, now including **code pointers** (function-pointer tables in
  game data): PC survives them only because PC code also lives under
  `0x40000000`; Android code will not. Any field found holding a code
  pointer is widened to host width via the established
  `#ifdef TARGET_PC` patch mechanism, or routed through a low trampoline
  slot — decided per-site by the inventory, recorded with evidence.
- An early milestone **spike**: build the decomp for arm64 with the
  data-image loader and boot the New Game opening on the RP6, before the
  renderer/packaging work. Fallback if segment-relocation loading proves
  unworkable at a specific site: per-site migration of that object into
  the window (the offline.patch pattern), not a wholesale redesign.
- Lift the `CMAKE_SIZEOF_VOID_P == 4` fatal guard behind the Android
  mode only; all other configurations keep the guard.
- Clang layout parity: replace the reliance on GCC's 32-byte array
  alignment with explicit alignment attributes (via `pc_prelude.h`,
  force-included into every decomp TU) plus compile-time offset asserts,
  so the NDK's required Clang toolchain reproduces GCC's layout.
- A runtime debug-mode trap verifies no out-of-window pointer enters a
  32-bit DMA field, and a startup check verifies every fixed mapping
  landed at its requested address, failing loudly otherwise.

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

### Android lifecycle

The PC build never faces GL context loss; Android does, and it is a
required subsystem, not an afterthought:

- **Backgrounding** destroys the EGL surface (often the context). The
  render layer gains a re-init path: all GL objects become rebuildable
  state — per-address framebuffer targets, native texture definitions,
  HD-pack uploads — reconstructed from the GS memory model on resume,
  which stays authoritative on the CPU side. This is verified by a test
  that forces surface destruction mid-run and requires identical frames
  before/after.
- **`onPause` auto-quicksave**: when the system takes the app away
  during player control, the existing quicksave system writes a safety
  snapshot (respecting its existing restrictions; when quicksave is not
  legal, only the re-init path protects the session).
- **Process death**: recovery is via that auto-quicksave on next launch
  — i.e., an OS kill resumes from the last pause point, not mid-frame.
  This consequence is accepted as the Android behavior (the alternative,
  surviving arbitrary process death mid-frame, is not feasible).

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
- **Windows**: the launcher menu and game are one exe (there is no
  separate launcher process), so the update applies **in-place from the
  running exe**: check on every open (before/at the launcher menu); if
  newer, download the `AshfordsHalberd-Campaign-Windows-x86.zip` asset,
  verify the asset digest from the release API, extract to a staged
  directory, rename the running exe (Windows permits renaming a running
  image), swap the install tree (settings/saves live in the per-user
  SDL pref dir, never in the install tree), and relaunch into the new
  version automatically; the old image is cleaned up on next start. No
  new bootstrapper process, no change to the user's startup flow.
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
  the opening run. **60 fps sustained on RP6 at auto scale with the
  Enhanced preset is a measured target, not a hard acceptance gate**; if
  on-device measurement lands below it, the plan's contingency ladder is
  (1) lower auto-scale threshold, (2) targeted optimization of measured
  hot spots, and only then (3) a NEON VU kernel for the measured hot
  path — which reopens the no-NEON non-goal and requires explicit user
  authorization. The performance spike runs early (before the full
  memory-model investment) so this decision is evidence-based.

### Testing and acceptance

- Host portable ctests keep passing unchanged (Windows x86 suite is the
  regression gate).
- Portable test binaries (`vu_portable`, `gsmem`, `bindings`, `parsers`,
  `presets`, ...) cross-compile for arm64 and run on the RP6 via an
  adb-driven ctest wrapper.
- **Parity gate, defined**: byte-exact frame dumps across different GPUs
  are not attainable (the determinism section of ARCHITECTURE.md already
  notes retail stale reads that shift with layout). Parity is proven in
  layers, each with a concrete pass criterion:
  1. **GS command-stream equality** (primary automated gate): the
     existing frame stream recorder (`gs/stream.c`) records the game's GS
     command stream on PC and Android; the opening-scene streams must
     match after removing stream-embedded host addresses. Divergence
     triggers a targeted arm64 stale-read hunt before any other work.
  2. **Raster comparison with tolerance**: `RECVX_DUMP_FRAMES` dumps on
     device, pulled via adb, compared against PC goldens for the opening
     scene across all video settings, passing when per-pixel deltas stay
     within a recorded tolerance and perceptual/histogram metrics match;
     the comparison tool and thresholds are a deliverable.
  3. **Golden-byte checks stay on PC**, where the GPU and layout are
     fixed; Android inherits correctness through layers 1–2.
- Touch UI verification: scripted show/hide from all three menus, gameplay
  via injected touch, gamepad-connect auto-hide.
- Updater verification: staged local release fixture (no live GitHub in
  tests) covering check, compare, download, swap/restart on Windows and
  prompt→install intent on Android.
- Acceptance: the app installs and passes the layers above on RP6, boots
  from the user's ISO, New Game opening 2400-frame run via touch and via
  built-in gamepad, settings changes persist, all video settings visibly
  function, saves round-trip, and both updaters update from a fixture
  release. **S24 Ultra acceptance is pending a user answer** on device
  availability: if the user can supply it, on-device acceptance runs
  there too; if not, Adreno 750 acceptance is capability-based (like
  Mali) rather than pretending a device was verified. Full campaign
  completion remains a playthrough claim beyond automated gates, same as
  PC.

## Calls made by the architect (correct or veto)

- GLES 3.2 (not Vulkan, not ANGLE) as the Android GL path.
- arm64-v8a only; `minSdk 26`.
- The 64-bit memory mechanism: game data image loaded into the mmap'd
  `0x30000000` window via a build-generated relocation table + small
  runtime loader; host pool at `0x50000000`; code stays in the PIE.
  Chosen over mass per-site static migration (the offline.patch pattern
  reserved as per-site fallback) because it preserves the identity layout
  the snapshot manifest, disc relocations and DMA tags all depend on.
- Windows auto-update applies **in-place from the running exe**
  (rename-running-image, staged swap, relaunch, cleanup on next start) —
  no new bootstrapper process and no change to the startup flow. Android
  asks with one tap because the OS forces a confirmation anyway.
- Android lifecycle: full GL re-init on surface/context loss + auto-
  quicksave on `onPause`; an OS-killed session resumes from the last
  pause quicksave, not mid-frame.
- Parity is proven by GS command-stream equality plus tolerant raster
  comparison (criteria above), not cross-GPU byte-exact dumps.
- 60 fps on RP6 is a measured target; the NEON contingency requires
  explicit user authorization and cannot be spent silently.
- ISO imported once via SAF into app-private storage (no persistent SAF
  permission juggling, no external-storage requirement).
- Touch overlay rendered with ImGui in the shared style layer.
- The offline-identity test contract is amended rather than abandoned.
- Update-check code lives in its own narrow module and is the only
  networking in the project.

## Left to the builder

- The pointer-field inventory itself (mechanism specified above; the
  sites, including code-pointer fields, and their dispositions are
  findings to produce and record).
- The data-image loader's build tooling details (ELF relocation table
  generation, arm64 manifest mode for `snapshot_manifest.py`).
- Exact GLES capability fallback thresholds (which devices get float
  targets) tuned against the device matrix as evidence accumulates.
- Touch layout details (button placement/size defaults) within the described
  shape, plus the touch automation hook design.
- Gradle/NDK integration specifics for SDL3 FetchContent on Android.
- The tolerance thresholds and comparison tool for the raster parity gate.
- Mali (and S24 Ultra if unavailable) on-device verification is
  capability-based until a device is available; the design does not block
  on owning one.
