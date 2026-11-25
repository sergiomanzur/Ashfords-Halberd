# Android arm64 32-Bit Pointer-Field Audit

This document records the checked-in inventory of every 32-bit field or choke point that receives a pointer in the Ashford's Halberd campaign build, the Android arm64 address-map region that guarantees each site stays lossless under 64-bit LP64, and any site fixed during the audit.

## Android arm64 Address Map (`campaign/pc/src/host/window_load.h`)

| Region | Address Range | Mechanism | Contents |
| --- | --- | --- | --- |
| Game data window | `[0x30000000, 0x40000000)` | `MAP_FIXED_NOREPLACE` + `android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)` + `android-window.ld` | `librecvx_game.so` ELF headers (`[0x30000000, ~0x30060000)`), `.dynamic`, `.got`, `.rodata` (writable), `.ahsnap`, `.trtab`, `.data`, `.bss` (`<= 0x40000000` asserted at link and runtime) |
| Game code range | `[0x40000000, 0x50000000)` | Same reservation, exec `PT_LOAD` at vaddr `0x10000000` | `librecvx_game.so` `.plt`, `.text`, `.eh_frame_hdr`, `.eh_frame` |
| DMA host pool | `[0x50000000, 0x70000000)` (256 MiB reserved at `[0x50000000, 0x60000000)`) | `MAP_FIXED_NOREPLACE` + 4 fixed 64 MiB slots (`ashford_pool_alloc`) | Dynamic host buffers read by EE DMA (`lang_glue.c` fonts, title plates, file pages) |
| PS2 scratchpad | `[0x70000000, 0x70004000)` | `MAP_FIXED_NOREPLACE` in `ps2_memory_init()` | 16 KiB EE scratchpad (`PS2_SPR_BASE`, `WORKBASE`) |
| Emulated IOP RAM | Virtual `[0x00000000, 0x00200000)` | Indexed into `iop_ram[2 MiB]` via `iop_ptr()` in `sif.c` | IOP heap & sound driver buffers (never cast directly to host pointer) |
| Host binary / heap / stack | `>= 0x0000004000000000` (ASLR) | Normal Android PIE / bionic allocator | Platform layer (`recvx_platform`), SDL3, ImGui, GL/audio state — **never permitted to enter a 32-bit DMA/game field** |

## Site-by-Site Pointer-Field Inventory

### 1. Game Statics & Uncached/Identity Aliasing (`librecvx_game.so`)
- **`njplus.c:739, 2039, 2471`** (`(int)fp | 0x30000000`, `(int)pHdr1 | 0x30000000`) — Covered by **Game data window** `[0x30000000, 0x40000000)`. Bit 28–29 (`0x30000000`) are already set on every game static and room-arena allocation inside `Ps2_malloc_mem`, so `(int)p | 0x30000000` is an identity.
- **`ps2_NinjaCnk.c:1494`** (`(int)pCnk | 0x30000000`) — Covered by **Game data window**.
- **`ps2_dummy.c:1607, 1626, 1639`** (`(int)&Ps2_PBUFF | 0x30000000`) — Covered by **Game data window** (`Ps2_PBUFF` is a `.bss` static inside `librecvx_game.so`).
- **`ps2_loadtim2.c:279-282` (`UncAddr`) & `301` (`UNCACHED`)** — Under `TARGET_PC`, `types.h:2469-2485` defines `UNCMASK = 0xffffffff`, `UNCBASE = 0`, `UNCACHED(x) = ((u_int)(x))`. All callers pass `static` buffers (`data[64]`, `Db`, `new_tags`) in `librecvx_game.so` — covered by **Game data window**.
- **`campaign/pc/glue/movie_glue.c:196`** (`(void*)((u_int)new_tags | UNCBASE)`) — `new_tags[64]` is a file-scope static in `movie_glue.c` linked into `librecvx_game.so` — covered by **Game data window**.
- **`campaign/pc/glue/lang_glue.c:946-967`** (`0x30000000u..0x40000000u` checks on `sys->mes_sp`, `rom->mesp`, `sys->rdtp`) — Covered by **Game data window**; non-window host tables (`sys_mes_fixed[]`) are explicitly detected and rebuilt on snapshot restore.

### 2. EE DMA Channel Registers, Tag Chains, and Host Pool
- **`campaign/pc/src/ps2/memory.c:128` (`dma_host[4].base`, `uint32_t`) & `ps2_dma_alloc()`** — **Fixed in Task 7**: on Android arm64, `mmap(NULL)` returns `>= 4 GiB` addresses which would truncate in `dma_host[].base`. Android delegates `ps2_dma_alloc()` to `ashford_pool_alloc()` in the **DMA host pool** `[0x50000000, 0x60000000)` and asserts `(uintptr_t)(uint32_t)(uintptr_t)p == (uintptr_t)p` (fatal otherwise).
- **`campaign/pc/glue/lang_glue.c:419, 442, 604, 652`** — All four host-buffer DMA callers allocate through `ps2_dma_alloc()` -> covered by **DMA host pool** `[0x50000000, 0x60000000)`.
- **`campaign/pc/src/ps2/dma.c:38-97` (`run_chain`, `tag[1]` 32-bit `ADDR`, `tadr`, `stack[2]`)** — Resolves every 32-bit address via `ps2_dma_ptr()`:
  - Active `dma_host[i]` slot -> **DMA host pool** `[0x50000000, 0x60000000)`.
  - `addr & 0x80000000` or `(addr & 0xF0000000) == 0x70000000` -> **PS2 scratchpad** `[0x70000000, 0x70004000)`.
  - `addr < 0x10000000` -> ORed with `0x30000000` into **Game data window** `[0x30000000, 0x40000000)`.
  - `0x30000000 <= addr < 0x40000000` -> identity in **Game data window**.
- **`campaign/pc/src/ps2/dma.c:102-113` (`sceDmaSend`, `sceDmaSendN`) & `ps2_loadtim2.c:732` (`DPUT_D2_TADR((int)tags)`)**:
  - `ps2_NinjaCnk.c:2225, 2518, 2746, 3039` pass scratchpad `0xF0003E80` or `ulpP | 0x80000000` -> **PS2 scratchpad**.
  - `ps2_dummy.c:1981, 2768, 2776` pass `Ps2_PBUFF` / `shadow_head` / `shadow_tail` -> **Game data window**.
  - **`ps2_dummy.c:2806-2828` (`Ps2Vu0ProgSend`, `Ps2Vu1ProgSend`) -> `ps2_vu0sub0`, `ps2_vu1sub0`, `ps2_vu1sub1`** — **Audit finding, fixed in Task 7**: previously defined in `campaign/pc/src/ps2/stubs.c` (`recvx_platform`), which sits at a high 64-bit ASLR address on Android and would have truncated in `sceDmaSend()`. Moved on Android into `campaign/pc/src/ps2/vu_stubs_android.c` linked directly into `librecvx_game.so` (**Game data window** `[0x30000000, 0x40000000)`), verified by `test_window_load`.
  - **Runtime guard**: `sceDmaSend()` and `sceDmaSendN()` on Android 64-bit halt via `host_fatal()` if passed any pointer `> UINT32_MAX`.
- **Platform data symbols imported by `librecvx_game.so`**: audited all 30 imported data symbols (`llvm-nm`). The remaining 27 (`g_config`, `g_settings`, `g_placer`, `g_vu0`, `ps2_regs`, `actor_models`, `lang_names`, `bind_rows`, etc.) are accessed exclusively through native 64-bit C pointers and never cast to 32-bit or passed to DMA.

### 3. SIF (EE <-> IOP) Transfers
- **`ps2_sg_sd.c:1977-2009` (`transDmaEEToIOP.data = src; transDmaEEToIOP.addr = dst;`) & `campaign/pc/src/ps2/sif.c:54-61` (`sceSifSetDma`)** — `data` (`u_int`) holds an EE sound buffer address inside `librecvx_game.so` (**Game data window**); `addr` (`u_int`) is an IOP offset mapped through `iop_ptr()` into `iop_ram[2 MiB]` (**Emulated IOP RAM**).
- **`campaign/pc/src/ps2/sif.c:26-32, 65-71` (`sceSifAllocIopHeap`, `sceSifGetOtherData`)** — Returns IOP offsets `[0x020000, 0x200000)` cast as `void*` tokens and immediately masked back via `iop_ptr((uint32_t)(uintptr_t)src)` (**Emulated IOP RAM**).

### 4. Code Pointers (INTC Handlers & Snapshot Validation)
- **`ps2_NaSystem.c:162` (`AddIntcHandler(2, vsync_func, 0)`)** — `vsync_func` is compiled into `librecvx_game.so` -> **Game code range** `[0x40000000, 0x50000000)`.
- **`campaign/pc/src/ps2/kernel.c:203-222` (`snapshot_platform_validate`)** — **Fixed in Task 7**: on 64-bit (`UINTPTR_MAX > 0xFFFFFFFFu`), validates non-NULL INTC handler pointers against `[ASHFORD_CODE_BASE, ASHFORD_CODE_LIMIT)` (`[0x40000000, 0x50000000)`), rejecting both data-window pointers and high ASLR pointers. 32-bit builds retain `[0x30000000, 0x40000000)` unchanged.

### 5. Snapshot Manifest Regions
- **`campaign/pc/src/host/snapshot_core.c` (`SnapshotRegion { uint32_t address, size; }`)** — Every region address is in `[0x30000000, 0x40000000)` (**Game data window**), validated against both bounds and CRC32 before capture/restore.

## Runtime Debug Trap (`RECVX_DMA_MAP_AUDIT`)
Setting `RECVX_DMA_MAP_AUDIT=1` enables a strict choke-point check in `ps2_dma_ptr()` (`campaign/pc/src/ps2/memory.c`) that halts via `host_fatal()` if any DMA address resolves outside the known Android map (game data window, game code range, active DMA host pool slot, or PS2 scratchpad). Together with the unconditional `> UINT32_MAX` guard in `sceDmaSend()` / `sceDmaSendN()`, out-of-window pointers cannot pass silently. Verified on device by `test_window_load`.
