/*
 * SIF (EE <-> IOP) on the host. IOP memory is emulated as a 2 MB buffer; DMA
 * transfers copy into it, RPCs are dispatched synchronously to host
 * implementations of the IOP modules (sound driver: audio/snd_iop.c).
 */
#include "ps2_platform.h"
#include <string.h>
#include <stdio.h>

#define IOP_RAM_SIZE (2 * 1024 * 1024)
uint8_t iop_ram[IOP_RAM_SIZE];
static uint32_t iop_heap_top = 0x020000;  /* heap: 0x20000-0xFFFFF; 0x100000+ belongs to the sound driver */

int iop_rpc_dispatch(u_int server, u_int fno, void* send, int ssize, void* receive, int rsize);
void snd_iop_status(uint32_t iop_addr);

void* iop_ptr(uint32_t iop_addr) { return &iop_ram[iop_addr & (IOP_RAM_SIZE - 1)]; }

void sceSifInitRpc(u_int mode) { (void)mode; }
int  sceSifRebootIop(const char* img) { (void)img; return 1; }
int  sceSifSyncIop(void) { return 1; }
int  sceSifInitIopHeap(void) { return 0; }
int  sceSifLoadModule(const char* name, int args, const char* argp) { (void)args; (void)argp; host_log("sif: load module %s\n", name); return 0; }
int  sceFsReset(void) { return 0; }

void* sceSifAllocIopHeap(u_int size)
{
    uint32_t a = (iop_heap_top + 63) & ~63u;
    if (a + size > IOP_RAM_SIZE) { host_log("sif: IOP heap exhausted\n"); return NULL; }
    iop_heap_top = a + size;
    return (void*)(uintptr_t)a;   /* IOP address */
}
int sceSifFreeIopHeap(void* addr) { (void)addr; return 0; }

int sceSifBindRpc(sceSifClientData* cd, u_int command, u_int mode)
{
    (void)mode;
    cd->command = command;
    cd->serve = (struct _sif_serve_data*)(uintptr_t)(0x1000 + command); /* non-NULL: bound */
    return 0;
}

int sceSifCallRpc(sceSifClientData* cd, u_int fno, u_int mode, void* send, int ssize, void* receive, int rsize,
                  sceSifEndFunc end_func, void* end_para)
{
    (void)mode;
    iop_rpc_dispatch(cd->command, fno, send, ssize, receive, rsize);
    if (end_func) end_func(end_para);
    return 0;
}

int sceSifCheckStatRpc(sceSifRpcData* cd) { (void)cd; return 0; }

u_int sceSifSetDma(sceSifDmaData* sdd, int len)
{
    for (int i = 0; i < len; i++) {
        /* EE -> IOP: data is an EE address, addr an IOP address */
        memcpy(iop_ptr(sdd[i].addr), (void*)(uintptr_t)sdd[i].data, (size_t)sdd[i].size);
    }
    return 1;
}
u_int isceSifSetDma(sceSifDmaData* sdd, int len) { return sceSifSetDma(sdd, len); }
int sceSifDmaStat(u_int id) { (void)id; return -1; /* completed */ }

int sceSifGetOtherData(sceSifReceiveData* rd, void* src, void* dest, int size, u_int mode)
{
    (void)rd; (void)mode;
    snd_iop_status((uint32_t)(uintptr_t)src);   /* the sound driver's status block: brought up to date */
    memcpy(dest, iop_ptr((uint32_t)(uintptr_t)src), (size_t)size);
    return 0;
}

/* Emulated IOP bytes and heap cursor; no host RPC callback pointers. */
#include "../audio/audio_snapshot.h"
typedef struct { uint32_t version, heap; unsigned char ram[IOP_RAM_SIZE]; } SifSnapshot;
int sif_snapshot_capture(SnapshotBlob* out) {
    SifSnapshot* p = calloc(1, sizeof(*p)); if (!p) return 0;
    p->version = 1; p->heap = iop_heap_top; memcpy(p->ram, iop_ram, sizeof(iop_ram));
    out->data = (unsigned char*)p; out->size = sizeof(*p); return 1;
}
int sif_snapshot_validate(const SnapshotBlob* in) {
    if (!in || !in->data || in->size != sizeof(SifSnapshot)) return 0;
    /* Avoid a multi-megabyte stack copy. */
    uint32_t v[2]; memcpy(v, in->data, sizeof(v));
    return v[0] == 1 && v[1] >= 0x20000 && v[1] <= IOP_RAM_SIZE;
}
void* sif_snapshot_stage(const SnapshotBlob* in) { if (!sif_snapshot_validate(in)) return NULL; void* p = malloc(in->size); if (p) memcpy(p, in->data, in->size); return p; }
void sif_snapshot_discard(void* p) { free(p); }
void sif_snapshot_commit(void* stage) { SifSnapshot* p = stage; memcpy(iop_ram, p->ram, sizeof(iop_ram)); iop_heap_top = p->heap; free(p); }
