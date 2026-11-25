# Android arm64 Foundation Backend

This document covers building, testing, and verifying the **Android arm64-v8a** campaign backend (Plan 1 foundation).

> **Scope note:** Plan 1 establishes and verifies the arm64 NDK toolchain, the windowed 32-bit-compatible memory model (`librecvx_game.so` + `window_load`), DMA host pool, Clang 32-byte buffer alignment parity, ELF64 snapshot manifest tooling, and the on-device headless test suite. It does **not** claim an on-screen Android APK boot yet — GLES 3.2 rendering, Android Activity/SAF ISO import, touch controls, and auto-updates follow in Plans 2–4.

## Measured Device Facts (Retroid Pocket 6, `cf916605`)

- **Hardware / OS:** Retroid Pocket 6 (Qualcomm QCS8550 / SD8 Gen 2, Adreno 740, GLES 3.2, Android 13, 64-bit-only `arm64-v8a`).
- **Low-memory reservations (`mmap` + `MAP_FIXED_NOREPLACE`):** `[0x30000000, 0x40000000)` (256 MiB), `[0x10000000, 0x20000000)`, `[0x50000000, 0x60000000)`, and `[0x70000000, 0x70004000)` all succeed and are writable (`mmap_min_addr` = 32 KiB).
- **`android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)`:** Verified on device. `librecvx_game.so` loads with exact `load_bias = 0x30000000`:
  - `PT_LOAD[1]` (`R--`, ELF metadata/dynsym): `[0x30000000, 0x3005a408)` (16 KiB aligned)
  - `PT_LOAD[2]` (`RW-`, `.dynamic`, `.got`, writable `.rodata`, `.ahsnap`, `.data`, `.bss`): `[0x30060000, 0x3dcde320)` (~221 MiB, 35 MiB headroom below `0x40000000`)
  - `PT_LOAD[3]` (`R-X`, `.plt`, `.text`, `.eh_frame_hdr`, `.eh_frame`): `[0x40000000, 0x4020a260)`
  - ELF64 snapshot manifest patched into `.ahsnap`: `393` merged regions (`230,571,941` bytes, CRC32 `0x9ccd47e2`, linked with `-Wl,-O0` so decomp `.rodata` string literals stay unmerged and attributed per TU), verified on device via `snapshot_core_available()` and `snapshot_core_signature()`.

## Android arm64 Address Map (`campaign/pc/src/host/window_load.h`)

| Range | Purpose |
| --- | --- |
| `[0x30000000, 0x40000000)` | Game data window (`librecvx_game.so` headers, writable `.rodata`, `.ahsnap`, `.data`, `.bss`, PS2 identity aliasing) |
| `[0x40000000, 0x50000000)` | Game code range (`librecvx_game.so` `.plt`/`.text`; INTC handler validation range on 64-bit) |
| `[0x50000000, 0x70000000)` | DMA host pool guard range (256 MiB reserved at `[0x50000000, 0x60000000)`, 4 fixed 64 MiB slots for `ps2_dma_alloc`) |
| `[0x70000000, 0x70004000)` | 16 KiB PS2 EE scratchpad (`PS2_SPR_BASE`, `MAP_FIXED_NOREPLACE`) |
| `>= 0x4000000000` (ASLR) | Host PIE executable/app, `recvx_platform`, SDL3, ImGui, heap — forbidden from 32-bit DMA/game fields |

See [docs/android-pointer-audit.md](android-pointer-audit.md) for the complete 32-bit pointer-field inventory and runtime `RECVX_DMA_MAP_AUDIT=1` trap.

## Prerequisites

- Android NDK r26+ (verified with `26.2.11394342` under `%LOCALAPPDATA%\Android\Sdk\ndk\`)
- Android `platform-tools` (`adb`) on `PATH` or under `%LOCALAPPDATA%\Android\Sdk\platform-tools\`
- CMake 3.20+, Ninja, Python 3
- Pinned decomp checkout (`upstream/recvx-campaign-decomp` at `4aac5133901da3821fe443b79d6cdd1e3b29e6da`) plus local Katana/CRI SDK headers for full builds (`-PlatformTestsOnly` does not need SDK headers)

## Build and On-Device Verification

Full build (`librecvx_game.so`, POST_BUILD ELF snapshot manifest patch, all arm64 test binaries) and complete on-device test sweep on a connected adb device:

```powershell
./tools/build-campaign-android.ps1
```

Useful options:

```powershell
# Target a specific adb serial
./tools/build-campaign-android.ps1 -Serial cf916605

# Cross-compile without running on-device tests
./tools/build-campaign-android.ps1 -SkipDeviceTests

# Build and run only the header-free platform component suite
./tools/build-campaign-android.ps1 -PlatformTestsOnly

# Run specific tests on device directly
python tools/run-android-ctest.py --build-dir build/campaign-android --test window_load --test reload
```

## On-Device Test Dispositions

The full-build runner executes **44 tests** (`35` portable + `window_load` + `8` decomp-linked headless tests):

- **38 PASS on Retroid Pocket 6:** `vu`, `vu_portable`, `bindings`, `modern`, `presets`, `pacing`, `ntex`, `gsmem`, `aa_tables`, `parsers`, `appear`, `variants`, `fileio`, `savestore`, `quickstore`, `snapshot_store`, `snapshot_core`, `snapshot_unpatched`, `game_rng`, `cheat_room_snapshot`, `first_person`, `renderer_snapshot_codec`, `cast`, `placer`, `biorand_opts`, `spoiler`, `import`, `lang`, `ashford_panel`, `window_load`, `rules`, `modern_glue`, `tps_pad`, `reload`, `quicksave`, `weapon_switch`, `movie_return`, `options_mouse`.
- **6 SKIP (deferred to Plan 2 Android app shell):** `pad_weapon_switch`, `pad_quicksave`, `rumble`, `test_audio_snapshot`, `test_audio_snapshot_decoder`, `test_audio_snapshot_synth` — require Android Java Activity / JNI initialization for `SDL_Init(SDL_INIT_GAMEPAD|SDL_INIT_AUDIO|0)`.
- **Not included in headless shell sweep (deferred to Plan 2 or host/ISO-gated):** `picture_gl` and `renderer_snapshot` (require EGL/GLES 3.2 context), `movies` (requires retail `RECVX_ISO`), `ashford_disc` (Windows `bcrypt` validator; Android hash port arrives in Plan 2), `settings`, `snapshot_manifest`, and `decomp_declarations` (host CMake/Python checks).

## Device Matrix

| Target | GPU / ABI | Status |
| --- | --- | --- |
| Retroid Pocket 6 | Adreno 740, GLES 3.2, `arm64-v8a`, Android 13 | **Measured & verified** (`mmap` probe, `android_dlopen_ext` probe, 38/38 runnable tests passing) |
| Samsung Galaxy S24 Ultra | Adreno 750, GLES 3.2, `arm64-v8a` | Owned by user; connect over adb for Plan 2–4 acceptance |
| Mali-G series (GLES 3.2 class) | Mali-G, `arm64-v8a` | Capability-based (16 KiB segment alignment + GLES 3.2 fallbacks in Plan 2) |

## Plan 2 Handoff Notes

- **64-bit pointer-field & struct-layout audit:** `test_reload` caught a 64-bit struct-punning bug in `sub1.c` `ItemSort()` (`((NJS_TEXNAME*)&st->pip[num1])->texaddr` assumed 4-byte `void* filename`), fixed in `campaign/patches/offline.patch`. Before booting retail `.RDX`/`.MLD` files in Plan 2, audit in-place disc relocations (`binfunc.c`, `dread.c`, `room.c`, `map.c`) where on-disc 32-bit pointer fields (e.g. `NJS_TEXNAME` = 12 bytes on disc vs 16 bytes under LP64, `ROM_WORK`, `NJS_CNK_OBJECT`) are overlaid directly onto C structs.
- **Reverse platform -> game symbols:** `test_window_load` isolates `librecvx_game.so` from `DT_NEEDED` using hidden-visibility stubs for the ~60 platform-to-game symbols plus `-Wl,-Bsymbolic` on `librecvx_game.so`. Plan 2's app shell will replace those test stubs with a thin runtime symbol table resolved via `dlsym(ashford_game_handle(), ...)` after `ashford_window_init()`.
