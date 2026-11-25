# Android arm64 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify the campaign backend for Android arm64 — toolchain, windowed memory model via `android_dlopen_ext` reserved ranges, and the portable test suite running on the Retroid Pocket 6 — so Plans 2–4 (app/GLES, touch, updaters) build on a proven foundation.

**Architecture:** Keep the existing campaign source tree and its identity memory model; add an Android arm64 cross-build in which the decomp+glue link as `librecvx_game.so` placed by linker script into a reservation at `0x30000000` (data in `[0x30000000,0x40000000)`, code in `[0x40000000,0x50000000)`), with the DMA host pool in a second reservation at `0x50000000`. The platform layer and tests stay in ordinary binaries; a loader module (`window_load.c`) is the product seam the Android app reuses.

**Tech Stack:** C/CMake (existing campaign build), Android NDK r26+ (Clang, arm64-v8a, `minSdk 26`), bionic `android_dlopen_ext`, adb from the host, Python for the on-device test runner.

**Spec:** `docs/superpowers/specs/2026-11-24-android-port-design.md` — the plan argues from its Memory model, Performance, and Testing/acceptance sections; read both.

## Global Constraints

- Decommit pin unchanged: `upstream/recvx-campaign-decomp` at `4aac5133901da3821fe443b79d6cdd1e3b29e6da`; decomp edits only via `campaign/patches/offline.patch` additions (TARGET_PC ifdefs, idempotent `git apply`).
- Dependency pins unchanged: SDL3 `release-3.4.16`, ImGui `v1.92.9b`, xxHash `v0.8.3`, bcdec `80859ed3b7afb1c527a2a99d70c61457bea72d0c`.
- Android targets: `arm64-v8a` only, `minSdk 26`, NDK r26+ (installed: `26.2.11394342`), PIE (no `-no-pie`, no `-Ttext-segment`).
- Address map constants (single source: `window_load.h`): window `[0x30000000,0x40000000)`, game code `[0x40000000,0x50000000)`, host pool guard range `[0x50000000,0x70000000)` — 256 MB actually reserved at `[0x50000000,0x60000000)` — scratchpad `0x70000000` size `0x4000` (existing `PS2_SPR_*`).
- All fixed mappings use `MAP_FIXED_NOREPLACE` and verify the returned address; failure is fatal with a clear message, never a silent fallthrough.
- The Windows x86 campaign build and its test suite must keep passing: after any task touching shared code, run `tools/build-campaign.ps1 -PlatformTestsOnly` (default WinLibs toolchain) and confirm green.
- No retail data, no SDK headers, no network code in any package or test; the offline-identity contract (`tests/offline_identity_tests.py`) keeps passing unchanged in this plan.
- Test binaries and evidence live under `local/` (untracked) and `build/`; nothing under `build/` is committed.
- Swarm closeout gate: every task that edits code lands only after the independent implementation-reviewer and critic gates pass on that task's diff and evidence (swarm-mode contract, `.zcode/session/swarm-mode.md`).

## Review Focus

1. **adb loses exit codes**: `adb shell test_bin` can report 0 even when the device process fails (older adb/shell quirk). Expected: a failing test must be reported FAIL. Test: Task 3 includes a deliberate `exit 3` probe binary the runner must report FAIL — before trusting any green run.
2. **Reservation silently ignored**: `android_dlopen_ext` may load at ASLR instead of the reserved range. Expected: load bias is exactly `0x30000000`. Test: Task 6's `window_load` asserts `dladdr` of a game symbol inside the window and the data-segment end below `0x40000000`; Task 2's probe gates this before any build depends on it.
3. **DMA heap truncation on arm64**: `mmap(NULL)` returns ≥4 GB addresses; `dma_host[].base` is `uint32_t`. Expected: every pool allocation is in `[0x50000000,0x70000000)` and `ps2_dma_ptr` resolves it by identity. Test: Task 7's pool/round-trip assertions in `window_load`.
4. **Clang/GCC layout drift**: NDK Clang does not give 32+-byte arrays GCC's 32-byte alignment that EE packet buffers rely on. Expected: every audited game buffer is 32-byte aligned on device. Test: Task 8's alignment audit in `window_load`, with per-symbol failures driving explicit `aligned(32)` fixes via offline.patch.
5. **False-green window audit**: an audit that only checks one probe symbol can pass while real statics sit outside the window. Expected: the audit enumerates the loaded library's segments, not just the probe. Test: Task 7's audit walks the game `.so`'s program headers and asserts the data segment fits the window; `snapshot_manifest` (Task 9) independently requires every `REQUIRED_SYMBOL` region inside it.

---

### Task 1: Android arm64 toolchain file, guard lift, and tests-only cross-compile

**Files:**
- Create: `campaign/cmake/android-arm64.cmake`
- Modify: `campaign/CMakeLists.txt:5-10` (sizeof guard)
- Modify: `campaign/pc/CMakeLists.txt` (guard x86-only flags; guard `ashford_campaign_disc` and `test_ashford_disc`)
- Modify: `campaign/pc/src/host/ashford_disc.cpp` (Android stub of `ashford_validate_disc`)
- Create: `tools/build-campaign-android.ps1`

**Interfaces:**
- Consumes: NDK's `build/cmake/android.toolchain.cmake` (chained), existing `ASHFORD_PLATFORM_TESTS_ONLY` mode.
- Produces: toolchain file accepting `-DANDROID_NDK=<path>` (no default guessing — the build script always passes it); build directory convention `build/campaign-android`; build script params `-NdkDir <path>` (default: newest `$env:LOCALAPPDATA/Android/Sdk/ndk/*`), `-PlatformTestsOnly`, `-BuildDirectory`, `-Jobs`, `-Serial` (adb serial, default unset = first device).

- [x] **Step 1: Write `campaign/cmake/android-arm64.cmake`** — sets `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-26`, then `include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")`; header comment documents GCC-layout parity being handled by explicit alignment (Task 8), not the compiler.
- [x] **Step 2: Write `tools/build-campaign-android.ps1`** — NDK discovery/validation (`<ndk>/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe` must exist), same decomp-pin check as `build-campaign.ps1`, `cmake -S <root> -B build/campaign-android -G Ninja -DASHFORD_CAMPAIGN=ON -DASHFORD_PLATFORM_TESTS_ONLY=ON -DCMAKE_TOOLCHAIN_FILE=campaign/cmake/android-arm64.cmake -DANDROID_NDK=<ndk>`, then `cmake --build`. No ctest call (host cannot execute arm64 tests; on-device runs are Task 3).
- [x] **Step 3: Run it and capture the failure modes** — Run: `./tools/build-campaign-android.ps1 -PlatformTestsOnly`. Expected: FAIL at the sizeof guard, then (after Step 4) at `-msse2 -mfpmath=sse` on arm64, then at `ashford_campaign_disc` (`windows.h`/`bcrypt`).
- [x] **Step 4: Guard each failure** — guard becomes `if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")` with the comment updated to name the windowed loader; the `-msse2 -mfpmath=sse` options are dropped for Android at **six** sites: the four test targets (`vu`, `vu_portable`, `gsmem`, `aa_tables`), the `recvx_game` compile options, and **`recvx_platform`'s compile options (pc/CMakeLists.txt:153 — built even in tests-only mode, so this is the first site the build actually hits)** (vu.c already guards SSE by architecture); `ashford_campaign_disc` compiles only off-Android, **and the `test_ashford_disc` target (pc/CMakeLists.txt:234-237, links bcrypt) is excluded from Android builds entirely**, with a temporary `ashford_validate_disc` stub under `#ifdef __ANDROID__` in `campaign/pc/src/host/ashford_disc.cpp` returning "unavailable on this platform until Plan 2" (replaced by a real hash port in Plan 2).
- [x] **Step 5: Verify the cross-compile** — Run: `./tools/build-campaign-android.ps1 -PlatformTestsOnly`. Expected: configure+build green; `llvm-readelf -h build/campaign-android/campaign/pc/test_vu_portable` shows `Machine: AArch64`.
- [x] **Step 6: Verify Windows non-regression** — Run: `tools/build-campaign.ps1 -PlatformTestsOnly`. Expected: green (guard changes must not touch the x86 path's behavior).
- [x] **Step 7: Commit** — `git add` the five files; message `feat(android): arm64 NDK toolchain and tests-only cross-compile`.

### Task 2: android_dlopen_ext reserved-range probe (mechanism gate)

**Files:**
- Create: `local/spikes/android-dlext-probe/probe.c`, `local/spikes/android-dlext-probe/lib.c` (throwaway, untracked — same convention as the mmap probe; the durable outcome is recorded in Task notes and drives Task 6)

**Interfaces:**
- Produces: a device-verified yes/no for `ANDROID_DLEXT_RESERVED_ADDRESS` on the RP6. If NO: stop and escalate to the architect before Task 5/6 — the design's recorded fallback (custom segment-relocation loader) is a different build.

- [x] **Step 1: Write a trivial library `libprobe.so`** exporting one function and one initialized global; build it arm64 with the NDK and a linker script putting its data at a fixed low segment offset (a miniature of the Task 5 layout).
- [x] **Step 2: Write `probe.c`** — mmap `MAP_FIXED_NOREPLACE` a reservation, `android_dlopen_ext` the library into it with `ANDROID_DLEXT_RESERVED_ADDRESS`, then assert: dlopen handle non-NULL; `dladdr(func)` and the global's address both inside the reservation at the expected offsets; calling the function returns its sentinel.
- [x] **Step 3: Run on the RP6** — push via adb, `chmod 755`, run. Expected: all assertions pass and print `PROBE RESULT: RESERVED LOAD OK`. Record the output verbatim in task notes.
- [x] **Step 4: Record the spike outcome** — Verified on Retroid Pocket 6 (`cf916605`): `reservation OK: [0x30000000, 0x50000000) PROT_NONE`, `android_dlopen_ext OK`, `probe_fn at 0x30001630 (in range: 1)`, `probe_global at 0x30003858 (in range: 1) value=0x51510000`, `PROBE RESULT: RESERVED LOAD OK`.

### Task 3: On-device ctest runner with honest exit codes

**Files:**
- Create: `tools/run-android-ctest.py`
- Modify: `tools/build-campaign-android.ps1` (call the runner after build)

**Interfaces:**
- Consumes: built test binaries under `build/campaign-android/campaign/pc/`; adb at `$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe`.
- Produces: CLI `python tools/run-android-ctest.py --build-dir <dir> [--serial <id>] [--test <name>]... [--list]`; a curated `PORTABLE_TESTS` list constant inside the runner; process exit code 0 only if every selected test PASSed or SKIPped (77 + no `RECVX_ISO` → SKIP, mirroring ctest semantics); per-test result lines `PASS|FAIL|SKIP <name>`.

- [x] **Step 1: Write the runner** — resolve binaries by ctest name (`test_<name>`); push to `/data/local/tmp/ashford-tests/<name>`; run via `adb shell "cd /data/local/tmp/ashford-tests && ./<name>; echo EXIT_CODE:$?"` parsing the echoed code (never trust adb's own exit code); per-test timeout (default 120 s, `timeout` shell wrapper); pull anything the test writes under its cwd; print the result table and a summary.
- [x] **Step 2: Verify it reports failure honestly** — push a binary that exits 3 (`adb shell` runs it), run the runner against it. Expected: `FAIL` with exit code 3 shown, runner exits nonzero.
- [x] **Step 3: First green** — run the known-pure set on the RP6: `--test vu_portable --test game_rng --test parsers --test aa_tables --test presets`. Expected: PASS for `vu_portable` at minimum (SSE already architecture-guarded); record which of the others fail and why — they are Task 4's input, not this task's scope.
- [x] **Step 4: Commit** — `git add tools/run-android-ctest.py tools/build-campaign-android.ps1`; message `feat(android): adb ctest runner with exit-code-verified results`.

### Task 4: Full portable suite green on device (bionic/POSIX gaps)

**Files:**
- Modify: `campaign/pc/src/host/log.c` (Android unwind branch), plus the specific platform/test sources whose failures Task 3 recorded — the exact set is an execution finding; each fix keeps a `#ifdef` branch so Linux/Windows paths are untouched.
- Modify: `tools/run-android-ctest.py` (extend `PORTABLE_TESTS` as tests go green)

**Interfaces:**
- Consumes: Task 3's runner and failure list.
- Produces: `PORTABLE_TESTS` containing every tests-only-buildable test that needs no GL context and no ISO, green on the RP6; Android crash-log path via NDK `<unwind.h>` (`_Unwind_Backtrace`) replacing glibc `execinfo.h`.

- [x] **Step 1: Extend the runner's default list and run the full portable set** — Expected: a bounded failure list; classify each as compile-time (missing header/API) or runtime (behavior).
- [x] **Step 2: Fix each failure at its root** — bionic lacks `execinfo.h` (use `<unwind.h>` under `__ANDROID__` in `log.c`/`host.c`); POSIX directory/file fallbacks under `__ANDROID__` in `savestore.c`, `import.c`, and test harnesses (`test_import.c`, `test_quickstore.c`, `test_savestore.c`) where SDL3's Android path helpers require Java/JNI initialization. No `#ifdef __ANDROID__` alters Windows/Linux branches.
- [x] **Step 3: Full portable suite green on device** — Run: `python tools/run-android-ctest.py --build-dir build/campaign-android`. Result on RP6 (`cf916605`): 29 passed, 0 failed, 6 skipped (recorded Java/SDL-init dispositions: `pad_weapon_switch`, `pad_quicksave`, `rumble`, `test_audio_snapshot`, `test_audio_snapshot_decoder`, `test_audio_snapshot_synth`), 35 total.
- [x] **Step 4: Windows non-regression** — Run: `tools/build-campaign.ps1 -PlatformTestsOnly`.
- [x] **Step 5: Commit** — message `feat(android): portable test suite green on arm64 device`.

### Task 5: `librecvx_game.so` with the window linker script

**Files:**
- Create: `campaign/cmake/android-window.ld`
- Modify: `campaign/pc/CMakeLists.txt` (Android target: `recvx_game`+glue as a shared library; platform stays out)
- Modify: `campaign/CMakeLists.txt` (Android full-build mode: keep patch+pin checks). The PE-only snapshot-manifest POST_BUILD lives at `campaign/pc/CMakeLists.txt:202-206` and is already `WIN32`-gated — Android skips it automatically; no work there beyond confirming the gate.

**Interfaces:**
- Consumes: `GAME_SOURCES` + `GLUE_SOURCES` glob lists (unchanged), `pc_prelude.h` force-include (unchanged).
- Produces: `librecvx_game.so` whose layout is: first `PT_LOAD` (data) at `0x10000` (64 KB-aligned — safe for 16 KB-page devices), game `.data/.bss/.rdata` (including `.ahsnap`, `.trtab` routed here) in `[0x10000, 0x10000000)`; second `PT_LOAD` (exec) at `0x10000000`; undefined platform symbols allowed (resolved from the loading process at `dlopen`); `-Wl,-z,max-page-size=16384`; no `RECVX_LAYOUT_PAD` on Android; full build configures with the decomp (headers fetched, patch applied).

- [ ] **Step 1: Write the linker script** — data segment first with the window budget as `ASSERT` (end of game data/bss `<= 0x10000000` in segment-relative terms, i.e. absolute `<= 0x40000000`), executable segment at `0x10000000`, `.ahsnap` kept.
- [ ] **Step 2: Add the Android game-library target** — same sources/flags as `recvx_game` plus `SHARED`, the linker script, `-fPIC` (decomp TUs compile PIC; the `(int)&x` idioms are unaffected because addresses become runtime window values), undefined symbols allowed. The `recvx` executable target is not built on Android (the app harness arrives in Plan 2; this plan's harness is Task 6's test binary).
- [ ] **Step 3: Verify full-build configure+link** — Run: `./tools/build-campaign-android.ps1` (full mode). Expected: decomp TUs compile with Clang (any Clang-vs-GCC hard errors are fixed with the minimal `#ifdef`/offline.patch hunk and recorded); `.so` links; `llvm-readelf -l` shows both PT_LOADs at the scripted addresses and no segment crossing `0x10000000` from the data side.
- [ ] **Step 4: Verify the statics budget holds** — data segment end below `0x10000000` (script ASSERT fires the build otherwise). Expected: link green with the standard `pc_prelude.h` budgets.
- [ ] **Step 5: Windows non-regression + commit** — Run: `tools/build-campaign.ps1 -PlatformTestsOnly`; expected green. Commit message `feat(android): windowed librecvx_game.so with fixed low segments`.

### Task 6: `window_load` module + on-device load/audit test

**Files:**
- Create: `campaign/pc/src/host/window_load.h`, `campaign/pc/src/host/window_load.c`
- Create: `campaign/pc/tests/test_window_load.c`
- Modify: `campaign/pc/CMakeLists.txt` (register the test on Android builds only), `tools/run-android-ctest.py` (`window_load` in the list)

**Interfaces:**
- Produces (exact header contract, single source of the address map):
  ```c
  #define ASHFORD_RESERVE_BASE   UINT32_C(0x30000000) /* reservation covers window + code */
  #define ASHFORD_RESERVE_SIZE   UINT32_C(0x20000000)
  #define ASHFORD_WINDOW_BASE    UINT32_C(0x30000000) /* game data + PS2 identities */
  #define ASHFORD_WINDOW_LIMIT   UINT32_C(0x40000000)
  #define ASHFORD_CODE_BASE      UINT32_C(0x40000000) /* game code range */
  #define ASHFORD_CODE_LIMIT     UINT32_C(0x50000000)
  #define ASHFORD_POOL_BASE      UINT32_C(0x50000000) /* dma host pool */
  #define ASHFORD_POOL_LIMIT     UINT32_C(0x70000000)
  int   ashford_window_init(void);            /* 0 ok; reserves both ranges, dlext-loads librecvx_game.so */
  void* ashford_game_handle(void);            /* dlopen handle after init, else NULL */
  void* ashford_pool_alloc(int slot, uint32_t len); /* one block per slot, replaces the slot's last; NULL outside the pool */
  void  ashford_pool_free(int slot);
  ```
  Failure contract: every failure is fatal with a message naming the range/API that failed (probe-verified behavior only; no silent fallbacks). Off-Android builds compile nothing from this pair (whole files guarded). Symbol resolution: the game `.so`'s undefined platform symbols (`pc_*`, `host_fatal`, SDL-backed hooks) resolve from the loading process — so the on-device test harness links with `-rdynamic` (`--export-dynamic`) so bionic's dlopen can bind them; the Plan 2 app does the same via its normal exported surface.

- [ ] **Step 1: Write `test_window_load.c`** (Android-only build) — asserts: `ashford_window_init()==0`; `dladdr` of the dlsym'd `Ps2_malloc_mem` inside `[0x30000000,0x40000000)`; game `.so` program-header walk: data PT_LOAD inside the window, exec PT_LOAD inside the code range; pool alloc at each slot lands in `[0x50000000,0x70000000)` and its truncated `uint32_t` value equals the pointer's low 32 bits. Register ctest `window_load`.
- [ ] **Step 2: Run on device — expect FAIL** — the loader doesn't exist yet; verify the test reports FAIL honestly via the Task 3 runner.
- [ ] **Step 3: Implement `window_load.c`** — two `MAP_FIXED_NOREPLACE` reservations with returned-address checks; `android_dlopen_ext(..., ANDROID_DLEXT_RESERVED_ADDRESS)` over `ASHFORD_RESERVE_BASE/SIZE`; load-bias verification by symbol address (Review Focus #2); 256 MB bump pool with the 4-slot `ps2_dma_alloc` contract.
- [ ] **Step 4: Run on device — expect PASS** — Run: `python tools/run-android-ctest.py --test window_load`. Expected: PASS.
- [ ] **Step 5: Windows non-regression + commit** — Run: `tools/build-campaign.ps1 -PlatformTestsOnly`; expected green (files are Android-guarded). Message `feat(android): window loader with reserved-range game library`.

### Task 7: Memory-model encoding in `memory.c` and `kernel.c`

**Files:**
- Modify: `campaign/pc/src/ps2/memory.c` (Android branch), `campaign/pc/src/ps2/kernel.c:203-222`, `campaign/pc/src/ps2_platform.h` (expose pool constants/prototypes)
- Create: `docs/android-pointer-audit.md` (the checked-in pointer-field inventory)

**Interfaces:**
- Consumes: Task 6's `ashford_pool_alloc`/`ashford_game_handle` and constants.
- Produces: `ps2_memory_init`'s `__ANDROID__` branch (scratchpad keeps the existing POSIX mmap; statics audit switches from PE headers to the game `.so`'s phdrs with `Ps2_malloc_mem` as the probe symbol — the platform lib itself lives high on Android, so the old `ps2_window_probe` check is Windows/Linux-only); `ps2_dma_alloc`'s Android branch delegating to `ashford_pool_alloc`; `snapshot_platform_validate`'s handler range check becoming `[ASHFORD_CODE_BASE, ASHFORD_CODE_LIMIT)` on 64-bit (unchanged constants on 32-bit).

- [ ] **Step 1: Extend `test_window_load.c`** — add: `ps2_memory_init()` on Android completes without `host_fatal`; the audit's data-segment bound assertion; the full `ps2_dma_alloc` → `dma_host[].base` → `ps2_dma_ptr` round-trip (`0x30123456` → identity, `0x00123456` → `0x30123456`, scratchpad address → `0x70000000|off`, pool block → identity); an INTC-handler-shaped validation exercising the new code range (a fake handler pointer inside `[0x40000000,0x50000000)` validates, one at a high address rejects); a high (≥4 GB) pointer passed to `ps2_dma_ptr` under the debug env var trips the trap.
- [ ] **Step 2: Run on device — expect FAIL** (audit still uses the PE/`_end` path; no trap exists).
- [ ] **Step 3: Implement the Android branches** — `dl_iterate_phdr` restricted to the game handle's link map (or `dladdr`-anchored phdr walk) for the window bound; pool delegation; range constants in `kernel.c` chosen by pointer size (`UINTPTR_MAX > 0xFFFFFFFF` → code range, else the legacy window); a `RECVX_DMA_MAP_AUDIT` env-gated trap in `ps2_dma_ptr` that `host_fatal`s on any resolved pointer outside the known map (window, code range, pool, scratchpad, IOP). No Windows/Linux line changes. The pointer-field inventory (spec "Implementation shape") is recorded as a checked-in audit document citing each 32-bit-receiving site and which map region can reach it; every site must be covered by a region or fixed per-site.
- [ ] **Step 4: Run on device — expect PASS** — full runner. Expected: `window_load` PASS, portable suite still green.
- [ ] **Step 5: Windows non-regression + commit** — Run: `tools/build-campaign.ps1 -PlatformTestsOnly` (and note: `snapshot_core`/`snapshot_unpatched` on host exercise the 32-bit validator path). Expected: green. Message `feat(android): window-aware memory audit, dma pool, snapshot code range`.

### Task 8: Clang layout parity — explicit alignment + audit

**Files:**
- Modify: `campaign/pc/include/pc_prelude.h` (alignment macro)
- Modify: `campaign/patches/offline.patch` (explicit `aligned` attributes on the specific decomp buffers found by the audit — TARGET_PC-scoped, idempotent)
- Modify: `campaign/pc/tests/test_window_load.c` (alignment audit)

**Interfaces:**
- Produces: `ASHFORD_ALIGN32` (`__attribute__((aligned(32)))`) applied to every game buffer GCC's 32-byte rule currently covers implicitly; an audited symbol list (`Ps2_malloc_mem`, `Ps2_tex_mem`, `Ps2_PBUFF`, `Ps2_ot_list`, `tbuf`, `ene`, `EnemyInfo`, `RequestList`, plus each newly found symbol) asserted 32-byte aligned on device. Spec note: the spec's "compile-time offset asserts" sub-element is superseded by this stronger on-device per-symbol alignment audit (runtime truth beats a host-side static check that cannot see the loaded `.so` layout); the supersession is recorded here.

- [ ] **Step 1: Add the failing alignment audit** — for each audited symbol (dlsym'd), assert `(addr & 31) == 0`. Run on device. Expected: FAIL on at least one GCC-implicitly-aligned buffer (the mingw toolchain comment predicts them); the failing set is this task's work list.
- [ ] **Step 2: Fix each failing buffer via offline.patch** — append idempotent hunks adding explicit alignment (default visibility of the symbols is unchanged); re-apply patch check is the configure step's job (`apply-campaign-patch.py` reverses cleanly only when applied).
- [ ] **Step 3: Run on device — expect PASS**; re-run the PC platform suite — alignment hunks must not shift any 32-bit layout behavior (GCC already aligned these; the attribute is a no-op there). Expected: both green.
- [ ] **Step 4: Commit** — message `fix(android): explicit 32-byte alignment for EE packet buffers`.

### Task 9: ELF snapshot manifest (quicksave foundation)

**Files:**
- Modify: `campaign/pc/tools/snapshot_manifest.py` (`--so` mode)
- Modify: `campaign/pc/CMakeLists.txt` (Android POST_BUILD patch step), `campaign/pc/tests/test_window_load.c` or new `campaign/pc/tests/test_snapshot_manifest_so.c` (on-device manifest availability check)

**Interfaces:**
- Consumes: the PE mode's constants and rules (`MAGIC='AHSMAP02'`, `REQUIRED_SYMBOLS`, `SAFE_GLUE`, `BASE/LIMIT`) — reused verbatim.
- Produces: `snapshot_manifest.py --so librecvx_game.so --nm <llvm-nm> --audit <json>` — selects window-segment regions for decomp+SAFE_GLUE symbols, validates every `REQUIRED_SYMBOL` covered and every region inside `[0x30000000,0x40000000)`, patches `.ahsnap` in the `.so` file with the same header/CRC format; runtime side (`snapshot_core.c`) unchanged (it already reads the patched bytes byte-by-byte).

- [ ] **Step 1: Write the failing on-device check** — `snapshot_core_available()` must be true and `snapshot_core_signature()` nonzero after a patched build. Run on device against the unpatched `.so`. Expected: FAIL (count=0).
- [ ] **Step 2: Implement `--so` mode** — ELF parsing via `llvm-readelf`/`llvm-nm` output (no new Python dependencies); same region merge/overlap rules; refuse a data segment crossing the window limit.
- [ ] **Step 3: Wire POST_BUILD and verify on device** — Run: `./tools/build-campaign-android.ps1` then the runner. Expected: manifest check PASS; `snapshot_unpatched` still validates the unpatched behavior (both patched and unpatched semantics covered on device).
- [ ] **Step 4: Windows non-regression + commit** — the Windows POST_BUILD path is untouched; run the PC platform suite. Expected: green. Message `feat(android): ELF snapshot manifest for arm64 quicksave`.

### Task 10: Full-build closeout, on-device ctest sweep, docs

**Files:**
- Modify: `tools/build-campaign-android.ps1` (full mode: build, patch manifest, run runner with the complete list), `README.md` (Android build section, one paragraph + pointer to new doc), `campaign/CHANGELOG.md` (entry), Create: `docs/android-backend.md` (build/run/evidence instructions, device matrix: RP6 measured; S24 Ultra owned, connect for acceptance; Mali capability-based)
- Modify: `tools/run-android-ctest.py` (final list)

**Interfaces:**
- Consumes: everything above.
- Produces: one command — `./tools/build-campaign-android.ps1` — that builds the full arm64 campaign (game `.so`, patched manifest, test binaries) and runs the complete on-device suite on the connected RP6; docs a builder/CI can follow without this chat.

- [ ] **Step 1: Run the full-build sweep** — full build + runner with: portable suite, `window_load`, manifest checks, plus the decomp-linked headless tests (`rules`, `modern_glue`, `tps_pad`, `reload`, `quicksave`, `weapon_switch`, `movie_return`, `options_mouse`) attempted on device. Expected: green, or a recorded per-test disposition — must-fix if the failure is memory-model/window related; deferred-to-Plan-2 (with reason) if it needs GL/SDL app lifecycle; never silently dropped.
- [ ] **Step 2: Windows x86 full non-regression** — Run: `tools/build-campaign.ps1` (full, existing verified toolchain). Expected: the 50/50-equivalent suite green; no Windows build flags changed.
- [ ] **Step 3: Write `docs/android-backend.md` + README/CHANGELOG pointers** — measured facts (mmap/dlext probes), address map, build commands, device matrix, what Plan 1 does and does not claim (no on-screen boot yet — that is Plan 2).
- [ ] **Step 4: Swarm closeout** — independent implementation reviewer over the whole Plan-1 diff + evidence, then a separate critic gate; fix every NEEDS_REVISION/REJECTED/BLOCKED item and re-run both gates on the latest diff (swarm-mode contract).
- [ ] **Step 5: Commit** — message `feat(android): arm64 foundation complete — suite green on device`; update the plan file's checkboxes along the way.
