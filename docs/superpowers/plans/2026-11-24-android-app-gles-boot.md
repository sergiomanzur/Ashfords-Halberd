# Android GLES 3.2, App Shell, Lifecycle & First Boot Implementation Plan (Plan 2)

**Goal:** Turn the verified Plan 1 `arm64-v8a` foundation into a real installable Android APK (`campaign/android/`) with a working OpenGL ES 3.2 renderer, GL context-loss recovery + `onPause` auto-quicksave, native SLUS-20184 disc validation, SAF ISO import, and LP64 on-disc binary realization — booting and playing the retail campaign on the connected Retroid Pocket 6 (`cf916605`) with verified GS command-stream and raster parity against Windows PC.

**Spec:** `docs/superpowers/specs/2026-11-24-android-port-design.md` (Renderer, Window and video settings mapping, Android lifecycle, Packaging and build, Performance, Testing and acceptance).

**Predecessor:** Plan 1 (`docs/superpowers/plans/2026-11-24-android-arm64-foundation.md`, completed at `6837194` on branch `android-arm64-foundation`).

## Global Constraints

- `upstream/recvx-campaign-decomp` pin stays `4aac5133901da3821fe443b79d6cdd1e3b29e6da`; all decomp changes live strictly in `campaign/patches/offline.patch` under `#ifdef TARGET_PC` (idempotent `apply-campaign-patch.py`).
- Dependency pins unchanged: SDL3 `release-3.4.16`, ImGui `v1.92.9b`, xxHash `v0.8.3`, bcdec `80859ed3b7afb1c527a2a99d70c61457bea72d0c`.
- Android target: `arm64-v8a` only, `minSdk 26`, NDK r26+ (`26.2.11394342`), PIE, `-Wl,-z,max-page-size=16384`.
- Single-source address map in `campaign/pc/src/host/window_load.h` (`[0x30000000, 0x40000000)` data window, `[0x40000000, 0x50000000)` game code, `[0x50000000, 0x60000000)` DMA host pool inside `[0x50000000, 0x70000000)` guard, `[0x70000000, 0x70004000)` scratchpad).
- Zero regressions on the Windows x86 build (`tools/build-campaign.ps1`, 50/50 tests) and `tests/offline_identity_tests.py`.
- No retail game data or ISOs committed anywhere in git.
- Scope boundary: Touch overlay controls belong to Plan 3; GitHub Releases auto-updaters belong to Plan 4.
- Closeout gate: independent Implementation Reviewer and separate Plan Critic must both pass before final closeout commit.

---

### Task 1: Portable SHA-1 & Android Disc Validator (`ashford_validate_disc` + `test_ashford_disc`)

**Files:**
- Modify: `src/core.cpp` (`#ifdef _WIN32` keeps Win32/BCrypt verbatim; `#else` adds portable RFC 3174 SHA-1 and POSIX/std::filesystem rename/copy helpers)
- Modify: `campaign/pc/src/host/ashford_disc.cpp` (remove temporary `__ANDROID__` stub so Android uses `ashford::validate_disc`)
- Modify: `campaign/pc/CMakeLists.txt` (enable `ashford_campaign_disc` and `test_ashford_disc` on Android without `bcrypt`)
- Modify: `tools/run-android-ctest.py` (add `ashford_disc` to `PORTABLE_TESTS`)

- [x] **Step 1:** Add portable SHA-1 + non-Win32 filesystem helpers in `src/core.cpp`, drop the Android stub in `ashford_disc.cpp`, enable `ashford_campaign_disc` + `test_ashford_disc` on Android in `campaign/pc/CMakeLists.txt`, and register `ashford_disc` in `tools/run-android-ctest.py`.
- [x] **Step 2:** Build and verify `ashford_disc` PASSES on Retroid Pocket 6 (`cf916605`) and Windows x86 (`-PlatformTestsOnly`, 41/41 passed including SHA-1 RFC 3174 vectors).
- [x] **Step 3:** Commit (`feat(android): portable SHA-1 and native SLUS-20184 disc validator`).

---

### Task 2: OpenGL ES 3.2 Renderer Backend

**Files:**
- Modify: `campaign/pc/src/render/gl_loader.h`, `campaign/pc/src/render/gl_loader.c` (`gl_load_es` / `glClearDepthf` fallback, `glGetStringi`, `glGetIntegerv`, ES capability flags `g_gl_es`, `g_gl_color_buffer_float`, `g_gl_clip_distance`)
- Modify: `campaign/pc/src/gs/picture_shader.h`, `campaign/pc/src/gs/gs_gl.c`, `campaign/pc/src/gs/aa.c`, `campaign/pc/src/gs/hdpack.c` (dual `#version 330 core` / `#version 320 es` shaders with `precision highp`, clip-distance extension + fragment `v_clip` fallback, `RGBA16F` vs `RGBA8` capability-gated targets + HUD/settings status, GLES-safe `glReadPixels`/`gs_snapshot_write_texture`/`gs_snapshot_read_texture` without `glGetTexImage`, CPU BGRA->RGBA & `bcdec` fallback for HD packs on ES)
- Modify: `campaign/pc/src/host/overlay.cpp` (`ImGui_ImplOpenGL3_Init("#version 300 es")` on GLES + RGBA8 capability indicator in Performance Graph HUD / settings)
- Modify: `campaign/pc/tests/test_picture_gl.c`, `campaign/pc/tests/test_renderer_snapshot.c` (request GLES 3.2 on `__ANDROID__`, keep GL 3.3 core on desktop)

- [x] **Step 1:** Implement GLES 3.2 support across `gl_loader`, `picture_shader.h`, `gs_gl.c`, `aa.c`, `hdpack.c`, and `overlay.cpp`.
- [x] **Step 2:** Verified live on Retroid Pocket 6 Adreno 740 (`es=1 float_rt=1 clip_dist=1 s3tc=1 bptc=1`) via headless EGL 3.2 pbuffer: `picture_gl` PASS, `renderer_snapshot` (including FXAA/SMAA/Deband) PASS in both `RGBA16F` and `RECVX_FORCE_RGBA8=1` modes; Windows x86 `-PlatformTestsOnly` 41/41 passed.
- [x] **Step 3:** Commit (`feat(android): OpenGL ES 3.2 renderer, shaders, and capability fallbacks`).

---

### Task 3: Android GL Context-Loss Lifecycle Recovery & `onPause` Auto-Quicksave

**Files:**
- Modify: `campaign/pc/src/gs/gs.h`, `campaign/pc/src/gs/gs_gl.c`, `campaign/pc/src/gs/aa.c`, `campaign/pc/src/gs/textures.c`, `campaign/pc/src/gs/hdpack.c` (`gs_renderer_context_lost`, `gs_renderer_context_restore`, `aa_reset_gl`, `ntex_reset_gl`, `hdpack_reset_gl`)
- Modify: `campaign/pc/src/host/overlay.cpp` (`overlay_context_lost`, `overlay_context_restore`)
- Modify: `campaign/pc/src/host/host.c`, `campaign/pc/src/host/render.c` (SDL3 Android background/foreground lifecycle event handling, `onPause` safety auto-quicksave when legal, pause render loop while surface is gone, recreate GL context + restore GL objects on resume)
- Create/Modify: `campaign/pc/tests/test_renderer_snapshot.c` (or dedicated `test_gl_context_loss.c`) verifying forced GL context destruction and recreation mid-run produces identical rendered frames before and after.

- [x] **Step 1:** Added `context_loss_recovery()` to `test_renderer_snapshot.c` testing both graceful (`context_alive=1`) and sudden (`context_alive=0`) real GL/EGL context destroy + recreate mid-run.
- [x] **Step 2:** Implemented `gs_renderer_context_lost`/`restore` and sub-module GL reset hooks (`aa`, `ntex`, `hdpack`, `overlay`) plus SDL3 background/foreground lifecycle & `onPause` auto-quicksave handling in `host.c`/`render.c`/`quicksave_ui.c`/`quicksave_glue.c`.
- [x] **Step 3:** Verified `renderer_snapshot` (including context-loss recovery in both `RGBA16F` and `RECVX_FORCE_RGBA8=1`) PASSES on Retroid Pocket 6 Adreno 740 (`0.4s`) and Windows x86 (`41/41`); commit (`feat(android): GL context-loss recovery and onPause auto-quicksave`).

---

### Task 4: Android Runtime Game-Symbol Bridge, APK Build Identity & Path Resolution

**Files:**
- Create: `campaign/pc/src/host/android_game_bridge.h`, `campaign/pc/src/host/android_game_bridge.c` (hidden-visibility function thunks + data pointers resolved via `dlsym(ashford_game_handle(), ...)` for all ~60 platform->game symbols, keeping `librecvx_game.so` out of `DT_NEEDED` and `.ahsnap` regions 100% unchanged)
- Modify: shared platform headers (`actors.h`, `cheat.h`, `enemies.h`, `lang.h`, `tps.h`, `import.h`) + `host.c`, `teleport.c`, `import/lang.c` (under `#ifdef ASHFORD_ANDROID_GAME_BRIDGE` only)
- Modify: `campaign/pc/src/host/window_load.h`, `campaign/pc/src/host/window_load.c` (`ashford_game_lib_path()`, handle `extractNativeLibs="false"` `base.apk!/...` paths, invoke bridge init)
- Modify: `campaign/pc/src/host/snapshot_store.c` (on `__ANDROID__`, hash `librecvx_game.so` via `ashford_game_lib_path()` instead of `/proc/self/exe`)
- Modify: `campaign/pc/CMakeLists.txt` (Android `libmain.so` shared library target for SDLActivity)
- Modify: `campaign/pc/tests/test_window_load.c` (assert every bridge symbol resolves inside `[0x30000000, 0x50000000)` and `snapshot_executable_hash()` hashes `librecvx_game.so`)

- [x] **Step 1:** Implement `android_game_bridge`, `libmain.so` CMake target, APK `snapshot_executable_hash()`, and `extractNativeLibs="false"` path handling.
- [x] **Step 2:** Verify `test_window_load` (61/61 bridge symbols, live calls, rebased `g_settings[]`/`keys[]`, `librecvx_game.so` & stored-ZIP SHA-256) + `snapshot_store` PASS on Retroid Pocket 6, zero leaked game symbols / zero `librecvx_game.so` in `libmain.so` `DT_NEEDED`, and Windows x86 `-PlatformTestsOnly` passes (41/41).
- [x] **Step 3:** Commit (`feat(android): runtime game-symbol bridge and libmain.so app target`).

---

### Task 5: Android Gradle APK (`campaign/android/`), SAF ISO Importer & Display Settings

**Files:**
- Create: `campaign/android/` (`settings.gradle`, `build.gradle`, `gradle.properties`, `app/build.gradle`, `app/src/main/AndroidManifest.xml`, `app/src/main/java/com/ashfordshalberd/app/AshfordActivity.java`, `app/src/main/res/...`)
- Create: `campaign/pc/src/host/android_saf.c` (JNI bridge for SAF `ACTION_OPEN_DOCUMENT` streaming copy into `<filesDir>/game/disc.iso`, intent extra `iso` / env passthrough for automated testing)
- Modify: `campaign/pc/src/host/host.c`, `campaign/pc/src/host/ashford_launcher.c`, `campaign/pc/src/host/settings.c` (Android GLES 3.2 context creation, `ashford_window_init()` at startup, SAF disc picker integration, Android launcher text/touch/gamepad navigation, honest Android display/presentation settings mapping)
- Modify: `tools/build-campaign-android.ps1` (support building the debug APK via Gradle alongside CMake native tests)

- [x] **Step 1:** Scaffold `campaign/android/` Gradle project wiring SDL3 `release-3.4.16` Java sources, `version.cmake` `versionName`/`versionCode`, `assets/branding/`, `libmain.so` + `librecvx_game.so` + `libSDL3.so`, `arm64-v8a`, `minSdk 26`, 16 KiB zip-aligned uncompressed native libs.
- [x] **Step 2:** Implement `AshfordActivity.java` + `android_saf.c` SAF ISO import and Android display/launcher wiring.
- [x] **Step 3:** Assembled APK with Gradle (`aapt2 dump badging`: `com.ashfordshalberd.app`, versionCode `90300`, `0.9.3b`, `minSdk 26`, `arm64-v8a`, GLES 3.2; `zipalign -c -P 16 4` OK for both APKs), installed on Retroid Pocket 6 (`cf916605`), verified live boot to the immersive `1920x1080` Ashford Launcher, AND built an `androidTest` instrumentation harness (framework `android.test.runner`, zero Maven deps, `libjni_test_*.so` staged ONLY in the androidTest APK) verifying live on the RP6: `pad_weapon_switch`, `pad_quicksave`, `rumble`, `test_audio_snapshot`, `test_audio_snapshot_decoder`, `test_audio_snapshot_synth` (7/7 PASS, 4.5s) plus the `bindings` virtual-gamepad block (dead in headless runs). Full on-device ctest roster: 41 passed, 0 failed, 6 skipped (47 total); Windows x86 `-PlatformTestsOnly`: 41/41; `tests/offline_identity_tests.py` 3/3 OK; `campaign/pc/tests/test_snapshot_manifest.py` 12/12 OK.
    - Found & fixed en route: `tmpfile()` is unusable in Android app processes (`test_bindings.c` now uses a cwd scratch file); the bindings reconnect block now numbers virtual pads relative to the first free SDL player index (the RP6's built-in gamepad holds player 0); the harness needs a headless `NativeTestActivity` context for SDL's Android file/audio JNI bridges.
- [x] **Step 4:** Commit (`feat(android): Gradle APK shell, SAF ISO import, and Android display mapping`).

---

### Task 6: LP64 On-Disc Binary Struct Realization

**Files:**
- Modify: `campaign/patches/offline.patch` (all decomp edits strictly `#ifdef TARGET_PC` and idempotent)
- Create: `campaign/pc/glue/lp64_realize.c`, `campaign/pc/glue/lp64_realize.h` (or inside `campaign/pc/glue/` linked into `librecvx_game.so` so unpacked structs allocate from a dedicated window arena inside `[0x30000000, 0x40000000)`, covered by `.ahsnap`!)
- Create: `campaign/pc/tests/test_lp64_realize.c` (unit tests against synthetic 32-bit PS2 binary payloads for `ROM_WORK`, `CUT_WORK`, `EVT_WORK`, `LGT_WORK`, `NJS_CNK_OBJECT`, `NJS_CNK_MODEL`, `NJS_MODEL`, `NJS_MESHSET`, `NJS_TEXLIST`, `NJS_TEXNAME`, `NJS_MOTION`, `NJS_MDATA2_MOD`)

- [x] **Step 1:** Audit all remaining on-disc binary struct overlays and hardcoded 32-bit struct sizes (`room.c`, `binfunc.c`, `dread.c`, `event.c`, `light.c`, `camera.c`, `map.c`, `adv.c`, `face_bh.c`, `ps2_texture.c`, `ps2_NaTextureFunction.c`). Full audit with exact file:line in `.superpowers/sdd/2026-11-24-android-app-gles-boot/progress.md` ("Task 6 Step 1 audit"). `main.c` `Ps2_PXLCONV` verified a CPU-side swizzle scratch (never enters DMA) - unchanged. ETTY_WORK/EF_WRK/ATR_WORK/POS/CAM_WRK are pointer-free (disc-resident on LP64); CAM_KEYF_WORK.recp is never read.
- [x] **Step 2:** Implement deterministic 32-bit disc -> LP64 window-arena struct realization + write unit tests in `test_lp64_realize.c`. (`campaign/pc/glue/lp64_realize.{h,c}`: ROM_WORK field-exact unpack into the runtime `romp` incl. the 16 relocated leading words + raw `dmp00..15`; CUT/LGT(lgtp+evlp)/EVC arrays + a 1-entry EVT view with the disc EVT word array kept for `Event_init`'s per-event offsets (`event.c:13091` reads it via `pc_lp64_evt_offset`, value-identical on 32-bit); Ninja trees via `pc_lp64_mlb_room/_session` + `pc_lp64_mnb_room/_session` hooks at every `bhMlbBinRealize`/`bhMnbBinRealize` call site (room vs session slots, dedup by disc base, `0xFFFFFFFF` sentinels realized per the decomp's exact per-field semantics, disc bytes never mutated). The arenas are the glue TU's static BSS inside `librecvx_game.so` (10 MiB room + 6 MiB session), covered by `.ahsnap` via `SAFE_GLUE + 'lp64_realize'`. PC glue disc reads fixed to disc-offset constants: `actor_glue.c`, `extras_glue.c`, `lang_glue.c`, `placer_glue.c`. Found & fixed en route, all caught by the new test on the RP6: a `(float)rd32` value-cast bug, `lp64_dedup_add` not storing its key, and `Angle` being `Sint32` -> disc `NJS_CNK_OBJECT` stride is 52, not 48.)
- [x] **Step 3:** Verify `test_lp64_realize` passes on RP6 (`PASS lp64_realize (0.1s)`; full roster 42 passed/0 failed/6 skipped, 48 total; `.ahsnap` regenerated 396 ranges/CRC32 2d899828; `snapshot_store` green on device) and Windows x86 non-regression stays green (`-PlatformTestsOnly` 42/42 incl. the pass-through branch, `movies` skip normal; `offline_identity_tests` 3/3; `test_snapshot_manifest` 12/12; `apply-campaign-patch.py` idempotent + reversible); commit (`feat(android): LP64 on-disc binary struct realization`).

---

### Task 7: Retail ISO Boot, Parity Verification & Plan 2 Closeout on Retroid Pocket 6

**Files:**
- Create: `tools/compare-frame-parity.py` (tolerant raster + perceptual/histogram comparison tool per design spec Layer 2)
- Modify: `docs/android-backend.md`, `docs/android-pointer-audit.md`, `campaign/CHANGELOG.md`, `README.md`, `docs/superpowers/plans/2026-11-24-android-app-gles-boot.md`

- [ ] **Step 1:** Stage the user's retail SLUS-20184 ISO into untracked app-private storage on RP6 (`cf916605`), boot the APK (including a run with `RECVX_DMA_MAP_AUDIT=1`), verify Title Screen -> New Game -> playable opening room; fix any LP64 / GLES runtime bugs uncovered on real retail assets.
- [ ] **Step 2:** Run Layer 1 (GS command-stream equality vs Windows PC) and Layer 2 (`RECVX_DUMP_FRAMES` tolerant raster comparison across video settings: scale, FXAA/SMAA, dithering/deband, picture controls, aspect, VSync/FPS limit), plus forced background/resume context-loss check and quicksave capture/restore round-trip on RP6. Record measured RP6 FPS/frame times.
- [ ] **Step 3:** Verify Windows x86 full suite (`50/50`), `tests/offline_identity_tests.py`, and `offline.patch` reversibility.
- [ ] **Step 4:** Independent Implementation Reviewer + separate Plan Critic closeout gate over the full Plan 2 diff and evidence.
- [ ] **Step 5:** Update docs & checkboxes and commit (`feat(android): GLES 3.2 APK, lifecycle recovery, and verified RP6 boot`).
