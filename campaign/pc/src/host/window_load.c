/* Android arm64 fixed-address window loader and DMA host pool.
 *
 * Reserves [0x30000000, 0x50000000) for librecvx_game.so (data/bss in
 * [0x30000000, 0x40000000), executable code in [0x40000000, 0x50000000)) and
 * [0x50000000, 0x60000000) (256 MiB inside [0x50000000, 0x70000000)) for the
 * 4-slot DMA host pool. All fixed mappings use MAP_FIXED_NOREPLACE and verify
 * exact placement; every failure is fatal via host_fatal.
 */
#if defined(__ANDROID__)
#define _GNU_SOURCE
#include "window_load.h"
#ifdef ASHFORD_ANDROID_GAME_BRIDGE
#include "android_game_bridge.h"
#endif
#include "../ps2_platform.h"
#include <android/dlext.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void* g_game_handle;
static char g_game_lib_path[PATH_MAX];
static int g_pool_ready;
static size_t g_pool_mapped[ASHFORD_POOL_SLOTS];

static size_t page_size_bytes(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    return (ps > 0) ? (size_t)ps : 4096u;
}

static void log_low4g_maps(void)
{
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[512];
    host_log("window_load: /proc/self/maps entries below 4 GiB:\n");
    while (fgets(line, sizeof(line), f)) {
        unsigned long long lo = 0, hi = 0;
        if (sscanf(line, "%llx-%llx", &lo, &hi) == 2 && lo < 0x100000000ULL) {
            host_log("  %s", line);
        }
    }
    fclose(f);
}

static int resolve_game_lib_path(char* out, size_t out_cap)
{
    const char* env_path = getenv("ASHFORD_GAME_LIB");
    if (env_path && env_path[0]) {
        if ((size_t)snprintf(out, out_cap, "%s", env_path) >= out_cap)
            return -1;
        return 0;
    }

    /* Check alongside the binary containing ashford_window_init (handles both
     * standalone test executables and an app's libmain.so in nativeLibraryDir). */
    Dl_info self_info;
    if (dladdr((const void*)(uintptr_t)&ashford_window_init, &self_info) &&
        self_info.dli_fname && self_info.dli_fname[0]) {
        const char* slash = strrchr(self_info.dli_fname, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - self_info.dli_fname);
            if (dir_len + sizeof("/librecvx_game.so") <= out_cap) {
                memcpy(out, self_info.dli_fname, dir_len);
                memcpy(out + dir_len, "/librecvx_game.so", sizeof("/librecvx_game.so"));
                /* When extractNativeLibs="false", dli_fname is "<apk>!/lib/arm64-v8a/libmain.so";
                 * access() cannot stat a zip entry, whereas bionic android_dlopen_ext() accepts it. */
                if (strstr(out, "!/") != NULL || access(out, R_OK) == 0)
                    return 0;
            }
        }
    }

    char exe_buf[PATH_MAX];
    ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (exe_len > 0) {
        exe_buf[exe_len] = '\0';
        const char* slash = strrchr(exe_buf, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - exe_buf);
            if (dir_len + sizeof("/librecvx_game.so") <= out_cap) {
                memcpy(out, exe_buf, dir_len);
                memcpy(out + dir_len, "/librecvx_game.so", sizeof("/librecvx_game.so"));
                if (access(out, R_OK) == 0)
                    return 0;
            }
        }
    }

    if ((size_t)snprintf(out, out_cap, "%s", "librecvx_game.so") >= out_cap)
        return -1;
    return 0;
}

typedef struct {
    uintptr_t fbase;
    uintptr_t probe_addr;
    int found;
    int data_segs;
    int exec_segs;
    uintptr_t data_end;
    uintptr_t exec_start;
    uintptr_t exec_end;
    char error[256];
} LoadVerifyCtx;

static int phdr_contains_addr(const struct dl_phdr_info* info, uintptr_t addr)
{
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD) continue;
        uintptr_t lo = (uintptr_t)info->dlpi_addr + (uintptr_t)ph->p_vaddr;
        uintptr_t hi = lo + (uintptr_t)ph->p_memsz;
        if (addr >= lo && addr < hi) return 1;
    }
    return 0;
}

static int verify_loaded_phdrs(struct dl_phdr_info* info, size_t sz, void* ud)
{
    LoadVerifyCtx* v = (LoadVerifyCtx*)ud;
    (void)sz;
    if ((uintptr_t)info->dlpi_addr != v->fbase)
        return 0;
    if (!phdr_contains_addr(info, v->probe_addr))
        return 0;

    v->found = 1;
    v->exec_start = UINTPTR_MAX;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD)
            continue;
        uintptr_t seg_start = (uintptr_t)info->dlpi_addr + (uintptr_t)ph->p_vaddr;
        uintptr_t seg_end = seg_start + (uintptr_t)ph->p_memsz;
        if ((ph->p_flags & PF_W) && (ph->p_flags & PF_X)) {
            snprintf(v->error, sizeof(v->error),
                     "PT_LOAD[%d] [0x%lx, 0x%lx) is both writable and executable",
                     i, (unsigned long)seg_start, (unsigned long)seg_end);
            return 1;
        }
        if (ph->p_flags & PF_X) {
            v->exec_segs++;
            if (seg_start < v->exec_start) v->exec_start = seg_start;
            if (seg_end > v->exec_end) v->exec_end = seg_end;
            if (seg_start < ASHFORD_CODE_BASE || seg_end > ASHFORD_CODE_LIMIT) {
                snprintf(v->error, sizeof(v->error),
                         "exec PT_LOAD[%d] [0x%lx, 0x%lx) outside code range [0x%08x, 0x%08x)",
                         i, (unsigned long)seg_start, (unsigned long)seg_end,
                         ASHFORD_CODE_BASE, ASHFORD_CODE_LIMIT);
                return 1;
            }
        } else {
            if (ph->p_flags & PF_W)
                v->data_segs++;
            if (seg_end > v->data_end)
                v->data_end = seg_end;
            if (seg_start < ASHFORD_WINDOW_BASE || seg_end > ASHFORD_WINDOW_LIMIT) {
                snprintf(v->error, sizeof(v->error),
                         "data PT_LOAD[%d] [0x%lx, 0x%lx) outside window [0x%08x, 0x%08x)",
                         i, (unsigned long)seg_start, (unsigned long)seg_end,
                         ASHFORD_WINDOW_BASE, ASHFORD_WINDOW_LIMIT);
                return 1;
            }
        }
    }
    return 1;
}

int ashford_window_init(void)
{
    if (g_game_handle)
        return 0;

    /* 1. Reserve [0x30000000, 0x50000000) for librecvx_game.so (window + code). */
    void* game_res = mmap((void*)(uintptr_t)ASHFORD_RESERVE_BASE, ASHFORD_RESERVE_SIZE,
                          PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (game_res != (void*)(uintptr_t)ASHFORD_RESERVE_BASE) {
        const char* reason = (game_res == MAP_FAILED) ? strerror(errno) : "unexpected address";
        log_low4g_maps();
        host_fatal("window_load: mmap(MAP_FIXED_NOREPLACE) failed for game reservation "
                   "[0x%08X, 0x%08X): got %p (%s)",
                   ASHFORD_RESERVE_BASE, ASHFORD_RESERVE_BASE + ASHFORD_RESERVE_SIZE,
                   game_res, reason);
    }

    /* 2. Reserve [0x50000000, 0x60000000) (256 MiB) for the DMA host pool. */
    void* pool_res = mmap((void*)(uintptr_t)ASHFORD_POOL_BASE, ASHFORD_POOL_RESERVE_SIZE,
                          PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (pool_res != (void*)(uintptr_t)ASHFORD_POOL_BASE) {
        const char* reason = (pool_res == MAP_FAILED) ? strerror(errno) : "unexpected address";
        log_low4g_maps();
        host_fatal("window_load: mmap(MAP_FIXED_NOREPLACE) failed for DMA pool reservation "
                   "[0x%08X, 0x%08X): got %p (%s)",
                   ASHFORD_POOL_BASE, ASHFORD_POOL_BASE + ASHFORD_POOL_RESERVE_SIZE,
                   pool_res, reason);
    }
    g_pool_ready = 1;

    /* 3. Load librecvx_game.so into the reserved window+code address range. */
    char lib_path[PATH_MAX];
    if (resolve_game_lib_path(lib_path, sizeof(lib_path)) != 0)
        host_fatal("window_load: failed to resolve path to librecvx_game.so");

    android_dlextinfo ext;
    memset(&ext, 0, sizeof(ext));
    ext.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
    ext.reserved_addr = (void*)(uintptr_t)ASHFORD_RESERVE_BASE;
    ext.reserved_size = ASHFORD_RESERVE_SIZE;

    void* handle = android_dlopen_ext(lib_path, RTLD_NOW, &ext);
    if (!handle) {
        host_fatal("window_load: android_dlopen_ext(%s, ANDROID_DLEXT_RESERVED_ADDRESS "
                   "[0x%08X, 0x%08X)) failed: %s",
                   lib_path, ASHFORD_RESERVE_BASE, ASHFORD_RESERVE_BASE + ASHFORD_RESERVE_SIZE,
                   dlerror());
    }

    /* 4. Verify bionic honored the exact load bias (never accept silent ASLR). */
    void* probe_sym = dlsym(handle, "Ps2_malloc_mem");
    if (!probe_sym)
        host_fatal("window_load: dlsym(Ps2_malloc_mem) failed on %s: %s", lib_path, dlerror());

    uintptr_t probe_addr = (uintptr_t)probe_sym;
    if (probe_addr < ASHFORD_WINDOW_BASE || probe_addr >= ASHFORD_WINDOW_LIMIT) {
        host_fatal("window_load: Ps2_malloc_mem landed at 0x%lx outside window [0x%08X, 0x%08X)",
                   (unsigned long)probe_addr, ASHFORD_WINDOW_BASE, ASHFORD_WINDOW_LIMIT);
    }

    Dl_info dli;
    memset(&dli, 0, sizeof(dli));
    if (!dladdr(probe_sym, &dli) || (uintptr_t)dli.dli_fbase != ASHFORD_RESERVE_BASE) {
        host_fatal("window_load: librecvx_game.so load bias mismatch: got 0x%lx, expected 0x%08X",
                   (unsigned long)(uintptr_t)dli.dli_fbase, ASHFORD_RESERVE_BASE);
    }

    /* 5. Audit all PT_LOAD segments of librecvx_game.so against the window/code map. */
    LoadVerifyCtx verify;
    memset(&verify, 0, sizeof(verify));
    verify.fbase = (uintptr_t)dli.dli_fbase;
    verify.probe_addr = probe_addr;
    dl_iterate_phdr(verify_loaded_phdrs, &verify);
    if (!verify.found)
        host_fatal("window_load: dl_iterate_phdr did not find librecvx_game.so at 0x%08X",
                   ASHFORD_RESERVE_BASE);
    if (verify.error[0])
        host_fatal("window_load: segment layout error: %s", verify.error);
    if (verify.data_segs < 1 || verify.exec_segs < 1) {
        host_fatal("window_load: unexpected segment counts (data=%d, exec=%d)",
                   verify.data_segs, verify.exec_segs);
    }

    const char* canon_path = (dli.dli_fname && dli.dli_fname[0]) ? dli.dli_fname : lib_path;
    snprintf(g_game_lib_path, sizeof(g_game_lib_path), "%s", canon_path);
    g_game_handle = handle;
#ifdef ASHFORD_ANDROID_GAME_BRIDGE
    ashford_game_bridge_init(g_game_handle);
#endif
    return 0;
}

void* ashford_game_handle(void)
{
    return g_game_handle;
}

const char* ashford_game_lib_path(void)
{
    return g_game_lib_path[0] ? g_game_lib_path : NULL;
}

void ashford_pool_free(int slot)
{
    if (!g_pool_ready || slot < 0 || slot >= ASHFORD_POOL_SLOTS)
        return;
    if (g_pool_mapped[slot] == 0)
        return;
    uintptr_t base = (uintptr_t)ASHFORD_POOL_BASE + (uintptr_t)slot * (uintptr_t)ASHFORD_POOL_SLOT_SIZE;
    size_t n = g_pool_mapped[slot];
    g_pool_mapped[slot] = 0;
    madvise((void*)base, n, MADV_DONTNEED);
    mprotect((void*)base, n, PROT_NONE);
}

void* ashford_pool_alloc(int slot, uint32_t len)
{
    if (!g_pool_ready || slot < 0 || slot >= ASHFORD_POOL_SLOTS)
        return NULL;

    ashford_pool_free(slot);
    if (len == 0)
        return NULL;

    size_t ps = page_size_bytes();
    if ((size_t)len > (size_t)ASHFORD_POOL_SLOT_SIZE)
        return NULL;
    size_t aligned = ((size_t)len + ps - 1u) & ~(ps - 1u);
    if (aligned < (size_t)len || aligned > (size_t)ASHFORD_POOL_SLOT_SIZE)
        return NULL;

    uintptr_t base = (uintptr_t)ASHFORD_POOL_BASE + (uintptr_t)slot * (uintptr_t)ASHFORD_POOL_SLOT_SIZE;
    if (base < ASHFORD_POOL_BASE || base + aligned > ASHFORD_POOL_LIMIT)
        return NULL;
    if ((uintptr_t)(uint32_t)base != base)
        return NULL;

    if (mprotect((void*)base, aligned, PROT_READ | PROT_WRITE) != 0)
        return NULL;

    memset((void*)base, 0, len);
    g_pool_mapped[slot] = aligned;
    return (void*)base;
}
#endif /* defined(__ANDROID__) */
