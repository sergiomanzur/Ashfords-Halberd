/* On-device verification of Android arm64 windowed loading and DMA pool. */
#define _GNU_SOURCE
#include "../src/host/window_load.h"
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

typedef struct {
    uintptr_t target_base;
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
    /* Match only the object whose name ends in librecvx_game.so (or contains our base). */
    if (!info->dlpi_name || !strstr(info->dlpi_name, "librecvx_game.so"))
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

    printf("test_window_load: PASS\n");
    return 0;
}
