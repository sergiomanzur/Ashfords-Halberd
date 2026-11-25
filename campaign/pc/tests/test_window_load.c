/* On-device verification of Android arm64 windowed loading and DMA pool. */
#define _GNU_SOURCE
#include "../src/host/window_load.h"
#include "../src/host/android_game_bridge.h"
#include "../src/host/cheat.h"
#include "../src/host/enemies.h"
#include "../src/host/lang.h"
#include "../src/host/room_snapshot.h"
#include "../src/host/settings.h"
#include "../src/host/tps.h"
#include "../src/ps2_platform.h"
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern int snapshot_hash_android_path(const char* path, unsigned char out[32]);
extern unsigned int RoomNameSwitch(unsigned int name);

static void wr16(unsigned char* p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void wr32(unsigned char* p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

/* Write a minimal valid uncompressed (STORE) ZIP with one entry for APK-entry hash testing. */
static int write_stored_zip(const char* zip_path, const char* entry_name, const unsigned char* data, uint32_t len)
{
    FILE* f = fopen(zip_path, "wb");
    if (!f) return 0;
    uint16_t nlen = (uint16_t)strlen(entry_name);
    unsigned char lhdr[30] = { 0 };
    wr32(lhdr + 0, UINT32_C(0x04034b50));
    wr16(lhdr + 4, 20);
    wr16(lhdr + 8, 0); /* STORE */
    wr32(lhdr + 18, len);
    wr32(lhdr + 22, len);
    wr16(lhdr + 26, nlen);
    uint32_t cd_off = 30u + (uint32_t)nlen + len;
    unsigned char cd[46] = { 0 };
    wr32(cd + 0, UINT32_C(0x02014b50));
    wr16(cd + 4, 20);
    wr16(cd + 6, 20);
    wr16(cd + 10, 0); /* STORE */
    wr32(cd + 20, len);
    wr32(cd + 24, len);
    wr16(cd + 28, nlen);
    wr32(cd + 42, 0); /* local header offset */
    uint32_t cd_size = 46u + (uint32_t)nlen;
    unsigned char eocd[22] = { 0 };
    wr32(eocd + 0, UINT32_C(0x06054b50));
    wr16(eocd + 8, 1);
    wr16(eocd + 10, 1);
    wr32(eocd + 12, cd_size);
    wr32(eocd + 16, cd_off);
    int ok = fwrite(lhdr, 1, sizeof(lhdr), f) == sizeof(lhdr) &&
             fwrite(entry_name, 1, nlen, f) == nlen &&
             fwrite(data, 1, len, f) == len &&
             fwrite(cd, 1, sizeof(cd), f) == sizeof(cd) &&
             fwrite(entry_name, 1, nlen, f) == nlen &&
             fwrite(eocd, 1, sizeof(eocd), f) == sizeof(eocd);
    return fclose(f) == 0 && ok;
}

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

typedef struct {
    uintptr_t target_base;
    uintptr_t probe_addr;
    int found;
    uintptr_t load_bias;
    int data_seg_count;
    int exec_seg_count;
    uintptr_t data_lo, data_hi;
    uintptr_t exec_lo, exec_hi;
    int seg_error;
} PhdrAudit;

static int audit_game_phdr_cb(struct dl_phdr_info* info, size_t sz, void* ud)
{
    PhdrAudit* a = (PhdrAudit*)ud;
    (void)sz;
    if ((uintptr_t)info->dlpi_addr != a->target_base)
        return 0;
    int contains_probe = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD) continue;
        uintptr_t lo = (uintptr_t)info->dlpi_addr + (uintptr_t)ph->p_vaddr;
        uintptr_t hi = lo + (uintptr_t)ph->p_memsz;
        if (a->probe_addr >= lo && a->probe_addr < hi) { contains_probe = 1; break; }
    }
    if (!contains_probe)
        return 0;

    a->found = 1;
    a->load_bias = (uintptr_t)info->dlpi_addr;
    a->data_lo = UINTPTR_MAX;
    a->data_hi = 0;
    a->exec_lo = UINTPTR_MAX;
    a->exec_hi = 0;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD)
            continue;
        uintptr_t start = a->load_bias + (uintptr_t)ph->p_vaddr;
        uintptr_t end = start + (uintptr_t)ph->p_memsz;
        printf("  PT_LOAD[%d]: [0x%08lx, 0x%08lx) flags=%c%c%c align=0x%lx\n",
               i, (unsigned long)start, (unsigned long)end,
               (ph->p_flags & PF_R) ? 'R' : '-',
               (ph->p_flags & PF_W) ? 'W' : '-',
               (ph->p_flags & PF_X) ? 'X' : '-',
               (unsigned long)ph->p_align);

        if ((ph->p_flags & PF_W) && (ph->p_flags & PF_X)) {
            fprintf(stderr, "FAIL: W^X violation on PT_LOAD[%d]\n", i);
            a->seg_error = 1;
        }
        if (ph->p_flags & PF_X) {
            a->exec_seg_count++;
            if (start < a->exec_lo) a->exec_lo = start;
            if (end > a->exec_hi) a->exec_hi = end;
            if (start < ASHFORD_CODE_BASE || end > ASHFORD_CODE_LIMIT) {
                fprintf(stderr, "FAIL: exec PT_LOAD[%d] [0x%lx, 0x%lx) outside code range\n",
                        i, (unsigned long)start, (unsigned long)end);
                a->seg_error = 1;
            }
        } else {
            if (ph->p_flags & PF_W) {
                a->data_seg_count++;
                if (start < a->data_lo) a->data_lo = start;
                if (end > a->data_hi) a->data_hi = end;
            }
            if (start < ASHFORD_WINDOW_BASE || end > ASHFORD_WINDOW_LIMIT) {
                fprintf(stderr, "FAIL: data/ro PT_LOAD[%d] [0x%lx, 0x%lx) outside window\n",
                        i, (unsigned long)start, (unsigned long)end);
                a->seg_error = 1;
            }
        }
    }
    return 1;
}

int main(void)
{
    printf("test_window_load: initializing Android arm64 window...\n");
    CHECK(ashford_game_handle() == NULL);
    CHECK(ashford_window_init() == 0);

    void* handle = ashford_game_handle();
    CHECK(handle != NULL);

    /* 1. Verify Ps2_malloc_mem lands inside the PS2 window [0x30000000, 0x40000000)
     *    and dladdr reports load base == ASHFORD_RESERVE_BASE (0x30000000). */
    void* sym_malloc = dlsym(handle, "Ps2_malloc_mem");
    CHECK(sym_malloc != NULL);
    uintptr_t a_malloc = (uintptr_t)sym_malloc;
    printf("  Ps2_malloc_mem  = 0x%08lx\n", (unsigned long)a_malloc);
    CHECK(a_malloc >= ASHFORD_WINDOW_BASE && a_malloc < ASHFORD_WINDOW_LIMIT);
    CHECK((a_malloc | 0x30000000UL) == a_malloc);

    Dl_info dli;
    memset(&dli, 0, sizeof(dli));
    CHECK(dladdr(sym_malloc, &dli) != 0);
    printf("  dladdr fbase    = 0x%08lx (%s)\n",
           (unsigned long)(uintptr_t)dli.dli_fbase,
           dli.dli_fname ? dli.dli_fname : "(null)");
    CHECK((uintptr_t)dli.dli_fbase == ASHFORD_RESERVE_BASE);

    /* 2. Verify game code symbol lands in [0x40000000, 0x50000000). */
    void* sym_main = dlsym(handle, "recvx_game_main");
    CHECK(sym_main != NULL);
    uintptr_t a_main = (uintptr_t)sym_main;
    printf("  recvx_game_main = 0x%08lx\n", (unsigned long)a_main);
    CHECK(a_main >= ASHFORD_CODE_BASE && a_main < ASHFORD_CODE_LIMIT);
    CHECK((uintptr_t)(uint32_t)a_main == a_main);

    /* 3. Walk librecvx_game.so program headers: verify all data segments stay in
     *    [0x30000000, 0x40000000) and exec segment stays in [0x40000000, 0x50000000). */
    PhdrAudit audit;
    memset(&audit, 0, sizeof(audit));
    audit.target_base = (uintptr_t)dli.dli_fbase;
    audit.probe_addr = a_malloc;
    dl_iterate_phdr(audit_game_phdr_cb, &audit);
    CHECK(audit.found);
    CHECK(!audit.seg_error);
    CHECK(audit.load_bias == ASHFORD_RESERVE_BASE);
    CHECK(audit.data_seg_count >= 1);
    CHECK(audit.exec_seg_count >= 1);
    CHECK(audit.data_lo >= ASHFORD_WINDOW_BASE && audit.data_hi <= ASHFORD_WINDOW_LIMIT);
    CHECK(audit.exec_lo >= ASHFORD_CODE_BASE && audit.exec_hi <= ASHFORD_CODE_LIMIT);

    /* 4. Verify DMA host pool allocations across all 4 slots:
     *    - Every block lands in [0x50000000, 0x70000000)
     *    - Truncated uint32_t equals the full 64-bit pointer (no arm64 truncation)
     *    - Memory is writable/readable and zero-initialized on replace
     *    - Invalid slots / oversized requests return NULL */
    void* ptrs[ASHFORD_POOL_SLOTS] = { 0 };
    for (int s = 0; s < ASHFORD_POOL_SLOTS; s++) {
        uint32_t len = (uint32_t)(65536u * (unsigned)(s + 1));
        void* p = ashford_pool_alloc(s, len);
        CHECK(p != NULL);
        uintptr_t addr = (uintptr_t)p;
        uint32_t addr32 = (uint32_t)addr;
        printf("  pool slot %d     = 0x%08lx (len=%u)\n", s, (unsigned long)addr, len);
        CHECK(addr >= ASHFORD_POOL_BASE && addr + len <= ASHFORD_POOL_LIMIT);
        CHECK((uintptr_t)addr32 == addr);
        /* Verify writable and non-overlapping with prior slots. */
        memset(p, 0xA0 + s, len);
        CHECK(((unsigned char*)p)[0] == (unsigned char)(0xA0 + s));
        CHECK(((unsigned char*)p)[len - 1] == (unsigned char)(0xA0 + s));
        for (int prev = 0; prev < s; prev++) {
            CHECK(ptrs[prev] != p);
            CHECK(((unsigned char*)ptrs[prev])[0] == (unsigned char)(0xA0 + prev));
        }
        ptrs[s] = p;
    }

    /* Replace slot 1: must return a fresh zeroed block in range. */
    void* p1_new = ashford_pool_alloc(1, 4096);
    CHECK(p1_new != NULL);
    CHECK((uintptr_t)p1_new >= ASHFORD_POOL_BASE && (uintptr_t)p1_new + 4096 <= ASHFORD_POOL_LIMIT);
    CHECK(((unsigned char*)p1_new)[0] == 0);
    CHECK(((unsigned char*)p1_new)[4095] == 0);

    /* Boundary checks. */
    CHECK(ashford_pool_alloc(-1, 4096) == NULL);
    CHECK(ashford_pool_alloc(ASHFORD_POOL_SLOTS, 4096) == NULL);
    CHECK(ashford_pool_alloc(0, 0) == NULL);
    CHECK(ashford_pool_alloc(0, ASHFORD_POOL_SLOT_SIZE + 4096u) == NULL);

    for (int s = 0; s < ASHFORD_POOL_SLOTS; s++)
        ashford_pool_free(s);

    /* 5. Task 7: ps2_memory_init() must succeed on Android via game-.so phdr audit
     *    and map the 16 KiB scratchpad at PS2_SPR_BASE (0x70000000). */
    ps2_memory_init();
    volatile uint32_t* spr = (volatile uint32_t*)(uintptr_t)PS2_SPR_BASE;
    spr[0] = 0xCAFEBABEu;
    spr[(PS2_SPR_SIZE / 4u) - 1u] = 0x12345678u;
    CHECK(spr[0] == 0xCAFEBABEu);
    CHECK(spr[(PS2_SPR_SIZE / 4u) - 1u] == 0x12345678u);

    /* 6. Task 7: ps2_dma_alloc -> dma_host[].base -> ps2_dma_ptr round-trip. */
    void* dma_blk = ps2_dma_alloc(0, 65536u);
    CHECK(dma_blk != NULL);
    uintptr_t dma_addr = (uintptr_t)dma_blk;
    CHECK(dma_addr >= ASHFORD_POOL_BASE && dma_addr + 65536u <= ASHFORD_POOL_LIMIT);
    CHECK((uintptr_t)(uint32_t)dma_addr == dma_addr);
    CHECK(ps2_dma_ptr((uint32_t)dma_addr) == dma_blk);
    CHECK(ps2_dma_ptr((uint32_t)dma_addr + 4096u) == (void*)(dma_addr + 4096u));
    CHECK(ps2_dma_ptr(0x30123456u) == (void*)(uintptr_t)0x30123456u);
    CHECK(ps2_dma_ptr(0x00123456u) == (void*)(uintptr_t)0x30123456u);
    CHECK(ps2_dma_ptr(0x70000120u) == (void*)(uintptr_t)0x70000120u);
    CHECK(ps2_dma_ptr(0x80000120u) == (void*)(uintptr_t)0x70000120u);

    /* 7. Task 7: VU stub DMA packets live inside librecvx_game.so window, not host high memory. */
    const char* vu_syms[] = { "ps2_vu0sub0", "ps2_vu1sub0", "ps2_vu1sub1" };
    for (int i = 0; i < 3; i++) {
        void* p = dlsym(handle, vu_syms[i]);
        CHECK(p != NULL);
        uintptr_t a = (uintptr_t)p;
        CHECK(a >= ASHFORD_WINDOW_BASE && a < ASHFORD_WINDOW_LIMIT);
    }

    /* 8. Task 7: kernel.c snapshot_platform_validate uses [0x40000000, 0x50000000)
     *    for INTC handlers on 64-bit, rejecting data-window and high (>=4 GiB) addresses. */
    {
        SnapshotBlob blob = { 0 };
        int (*valid_handler)(int) = (int (*)(int))(uintptr_t)a_main;
        int id = AddIntcHandler(2, valid_handler, 0);
        CHECK(id > 0);
        CHECK(snapshot_platform_capture(&blob));
        CHECK(snapshot_platform_validate(&blob));
        free(blob.data);
        blob.data = NULL; blob.size = 0;
        RemoveIntcHandler(2, id);

        /* High >=4 GiB host address must be rejected. */
        int (*high_handler)(int) = (int (*)(int))(uintptr_t)&audit_game_phdr_cb;
        CHECK((uintptr_t)high_handler >= (uintptr_t)ASHFORD_CODE_LIMIT);
        id = AddIntcHandler(2, high_handler, 0);
        CHECK(id > 0);
        CHECK(snapshot_platform_capture(&blob));
        CHECK(!snapshot_platform_validate(&blob));
        free(blob.data);
        blob.data = NULL; blob.size = 0;
        RemoveIntcHandler(2, id);

        /* Data-window address [0x30000000, 0x40000000) must also be rejected as code on 64-bit. */
        int (*data_handler)(int) = (int (*)(int))(uintptr_t)0x30010000u;
        id = AddIntcHandler(2, data_handler, 0);
        CHECK(id > 0);
        CHECK(snapshot_platform_capture(&blob));
        CHECK(!snapshot_platform_validate(&blob));
        free(blob.data);
        blob.data = NULL; blob.size = 0;
        RemoveIntcHandler(2, id);
    }

    /* 9. Task 7: RECVX_DMA_MAP_AUDIT debug trap halts on out-of-map addresses and >=4 GiB pointers. */
    {
        fflush(stdout);
        /* Valid addresses must pass even with audit enabled. */
        pid_t pid = fork();
        CHECK(pid >= 0);
        if (pid == 0) {
            setenv("RECVX_DMA_MAP_AUDIT", "1", 1);
            ps2_dma_audit_refresh();
            if (ps2_dma_ptr(0x30123456u) != (void*)(uintptr_t)0x30123456u) _exit(2);
            if (ps2_dma_ptr(0x00123456u) != (void*)(uintptr_t)0x30123456u) _exit(3);
            if (ps2_dma_ptr(0x70000100u) != (void*)(uintptr_t)0x70000100u) _exit(4);
            if (ps2_dma_ptr((uint32_t)dma_addr) != dma_blk) _exit(5);
            _exit(0);
        }
        int status = 0;
        CHECK(waitpid(pid, &status, 0) == pid);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

        /* Out-of-map 32-bit address (0x65000000) must trip host_fatal. */
        pid = fork();
        CHECK(pid >= 0);
        if (pid == 0) {
            setenv("RECVX_DMA_MAP_AUDIT", "1", 1);
            ps2_dma_audit_refresh();
            (void)ps2_dma_ptr(0x65000000u);
            _exit(0);
        }
        status = 0;
        CHECK(waitpid(pid, &status, 0) == pid);
        CHECK((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status));

        /* High >=4 GiB host heap pointer passed to sceDmaSendN must trip host_fatal. */
        pid = fork();
        CHECK(pid >= 0);
        if (pid == 0) {
            setenv("RECVX_DMA_MAP_AUDIT", "1", 1);
            ps2_dma_audit_refresh();
            void* high_buf = malloc(64);
            if (!high_buf || (uintptr_t)high_buf <= UINT32_MAX) _exit(99);
            sceDmaSendN((void*)(uintptr_t)0x1000a000u, high_buf, 1);
            _exit(0);
        }
        status = 0;
        CHECK(waitpid(pid, &status, 0) == pid);
        CHECK((WIFEXITED(status) && WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 99) || WIFSIGNALED(status));
    }

    ps2_dma_alloc(0, 0);

    /* 10. Task 8: Clang layout parity — audited EE/DMA game buffers must be 32-byte aligned. */
    {
        static const char* const aligned_syms[] = {
            "Ps2_malloc_mem",
            "Ps2_tex_mem",
            "Ps2_PBUFF",
            "Ps2_ot_list",
            "Ps2_DRAW_TMP",
            "Ps2_tex_save",
            "mbuf",
            "palbuf",
            "clear_buff",
            "tbuf",
            "ene",
            "eff",
            "sysp",
            "np",
            "EnemyInfo",
            "RequestList",
        };
        int align_fails = 0;
        for (size_t i = 0; i < sizeof(aligned_syms) / sizeof(aligned_syms[0]); i++) {
            void* sym = dlsym(handle, aligned_syms[i]);
            if (!sym) {
                fprintf(stderr, "FAIL: missing audited symbol %s\n", aligned_syms[i]);
                align_fails++;
                continue;
            }
            uintptr_t addr = (uintptr_t)sym;
            if (addr < ASHFORD_WINDOW_BASE || addr >= ASHFORD_WINDOW_LIMIT) {
                fprintf(stderr, "FAIL: %s at 0x%lx outside window\n",
                        aligned_syms[i], (unsigned long)addr);
                align_fails++;
            }
            if ((addr & 31u) != 0) {
                fprintf(stderr, "FAIL: %s at 0x%08lx is not 32-byte aligned (mod 32 = %lu)\n",
                        aligned_syms[i], (unsigned long)addr, (unsigned long)(addr & 31u));
                align_fails++;
            }
        }
        CHECK(align_fails == 0);
    }

    /* 11. Task 9: ELF snapshot manifest must be patched into librecvx_game.so .ahsnap. */
    {
        int (*snap_avail)(void) = (int (*)(void))dlsym(handle, "snapshot_core_available");
        uint32_t (*snap_sig)(void) = (uint32_t (*)(void))dlsym(handle, "snapshot_core_signature");
        CHECK(snap_avail != NULL);
        CHECK(snap_sig != NULL);
        int avail = snap_avail();
        uint32_t sig = snap_sig();
        printf("  snapshot_core   = available=%d signature=0x%08x\n", avail, sig);
        CHECK(avail != 0);
        CHECK(sig != 0);
    }

    /* 12. Plan 2 Task 4: Runtime game-symbol bridge resolves all 16 data + 45 func symbols,
     *     rebases g_settings[]/keys[] pointers into librecvx_game.so, and dispatches live calls. */
    {
        AshfordBridgeAudit ba;
        ashford_game_bridge_audit(&ba);
        printf("  game_bridge     = ready=%d data=%d/%d func=%d/%d\n",
               ba.ready, ba.data_in_window, ba.data_count, ba.func_in_code, ba.func_count);
        CHECK(ba.ready == 1);
        CHECK(ba.data_count == 16 && ba.data_in_window == 16);
        CHECK(ba.func_count == 45 && ba.func_in_code == 45);

        /* Direct bridged data access must alias librecvx_game.so's live objects. */
        TpsConfig* raw_tps = (TpsConfig*)dlsym(handle, "g_tps");
        EnemyOpts* raw_eo = (EnemyOpts*)dlsym(handle, "g_enemy_opts");
        CheatState* raw_cheat = (CheatState*)dlsym(handle, "g_cheat");
        CHECK(raw_tps != NULL && raw_eo != NULL && raw_cheat != NULL);
        CHECK(&g_tps == raw_tps);
        CHECK(&g_enemy_opts == raw_eo);
        CHECK(&g_cheat == raw_cheat);

        int saved_fov = g_tps.fov;
        g_tps.fov = 93;
        CHECK(raw_tps->fov == 93);
        g_tps.fov = saved_fov;

        /* Live calls through hidden forwarding thunks into librecvx_game.so. */
        CHECK(enemy_opts() == raw_eo);
        CHECK(spawn_count() > 0);
        CHECK(spawn_name(0) != NULL && spawn_name(0)[0] != '\0');
        lang_scan();
        CHECK(lang_installed(LANG_EN) == 1);
        CHECK(pc_in_game() == 0);
        CHECK(room_snapshot_busy() == 0);
        CHECK(RoomNameSwitch(0) == 0);

        /* Rebased settings table pointers must target live librecvx_game.so fields (never sentinels). */
        int checked_variety = 0, checked_fov = 0, checked_inf_hp = 0;
        for (int i = 0; i < g_settings_n; i++) {
            if (!strcmp(g_settings[i].label, "Randomizer variety")) {
                CHECK(g_settings[i].val == &raw_eo->variety);
                checked_variety = 1;
            } else if (!strcmp(g_settings[i].label, "Field of view")) {
                CHECK(g_settings[i].val == &raw_tps->fov);
                checked_fov = 1;
            } else if (!strcmp(g_settings[i].label, "Infinite health")) {
                CHECK(g_settings[i].val == (int*)&raw_cheat->inf_health);
                checked_inf_hp = 1;
            }
        }
        CHECK(checked_variety && checked_fov && checked_inf_hp);
    }

    /* 13. Plan 2 Task 4: snapshot_executable_hash() hashes librecvx_game.so (not /proc/self/exe),
     *     and snapshot_hash_android_path() handles uncompressed "<apk>!/<entry>" ZIP entries. */
    {
        const char* lib_path = ashford_game_lib_path();
        CHECK(lib_path != NULL && lib_path[0] != '\0');
        printf("  game_lib_path   = %s\n", lib_path);

        unsigned char h_exec[32], h_lib[32], h_self[32];
        CHECK(snapshot_executable_hash(h_exec));
        CHECK(snapshot_hash_android_path(lib_path, h_lib));
        CHECK(snapshot_hash_android_path("/proc/self/exe", h_self));
        CHECK(memcmp(h_exec, h_lib, 32) == 0);
        CHECK(memcmp(h_exec, h_self, 32) != 0);

        /* Verify stored ZIP entry hashing matches raw payload SHA-256 ("abc" test vector). */
        const char* tmp_zip = "/data/local/tmp/ashford_zip_hash_test.apk";
        const char* entry = "lib/arm64-v8a/librecvx_game.so";
        const unsigned char payload[3] = { 'a', 'b', 'c' };
        /* SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
        const unsigned char sha256_abc[32] = {
            0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
            0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
        };
        CHECK(write_stored_zip(tmp_zip, entry, payload, 3));
        char zip_spec[256];
        snprintf(zip_spec, sizeof(zip_spec), "%s!/%s", tmp_zip, entry);
        unsigned char h_zip[32];
        CHECK(snapshot_hash_android_path(zip_spec, h_zip));
        CHECK(memcmp(h_zip, sha256_abc, 32) == 0);
        unlink(tmp_zip);
    }

    printf("test_window_load: PASS\n");
    return 0;
}
