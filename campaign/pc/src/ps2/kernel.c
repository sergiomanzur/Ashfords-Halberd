/*
 * EE kernel services used by the game, on the host.
 *
 * The game runs single-threaded on the host. Interrupts are delivered
 * synchronously at platform-layer entry points (VBLANK waits, sync calls), so
 * game state is never touched concurrently; timing follows real NTSC rates.
 */
#include "ps2_platform.h"
#include "../host/prof.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../host/room_snapshot.h"

#define INTC_VBLANK_S 2
#define INTC_VBLANK_E 3
#define MAX_HANDLERS  8

#define NTSC_VBLANK_NS 16683350ull /* 1 / 59.94 Hz */
#define NTSC_HSYNC_HZ  15734ull

typedef int (*IntcHandler)(int);

static struct {
    IntcHandler fn[MAX_HANDLERS];
    int enabled;
} intc[16];

static uint64_t next_vblank_ns;
static uint64_t vblank_counter;

uint64_t ps2_vblank_count(void) { return vblank_counter; }

int AddIntcHandler(int cause, int (*handler)(int), int next)
{
    (void)next;
    for (int i = 0; i < MAX_HANDLERS; i++) {
        if (!intc[cause & 15].fn[i]) { intc[cause & 15].fn[i] = handler; return i + 1; }
    }
    return -1;
}

int RemoveIntcHandler(int cause, int id)
{
    if (id >= 1 && id <= MAX_HANDLERS) intc[cause & 15].fn[id - 1] = NULL;
    return 0;
}

int EnableIntc(int cause)  { int old = intc[cause & 15].enabled; intc[cause & 15].enabled = 1; return old ? 0 : 1; }
int DisableIntc(int cause) { int old = intc[cause & 15].enabled; intc[cause & 15].enabled = 0; return old ? 1 : 0; }
void ExitHandler(void) {}

static void raise_intc(int cause)
{
    if (!intc[cause].enabled) return;
    for (int i = 0; i < MAX_HANDLERS; i++)
        if (intc[cause].fn[i]) intc[cause].fn[i](cause);
}

static void deliver_vblank(void)
{
    vblank_counter++;
    ps2_regs.t0_count += 525 / 2; /* T0 counts H-blanks in the game's setup; approximate per field */
    raise_intc(INTC_VBLANK_S);
    raise_intc(INTC_VBLANK_E);
}

/* Deliver every VBLANK whose time has passed. Called from platform entry points. */
void ps2_poll_interrupts(void)
{
    uint64_t now = SDL_GetTicksNS();
    if (!next_vblank_ns) next_vblank_ns = now + NTSC_VBLANK_NS;
    /* after long stalls (loading, debugger) do not replay a burst of interrupts */
    if (now > next_vblank_ns + 8 * NTSC_VBLANK_NS) next_vblank_ns = now;
    while (now >= next_vblank_ns) {
        deliver_vblank();
        next_vblank_ns += NTSC_VBLANK_NS;
    }
}

/* Block until the next VBLANK interrupt has been delivered. */
void ps2_wait_vblank(void)
{
    uint64_t target;
    prof_push(PROF_WAIT);
    if (!next_vblank_ns) next_vblank_ns = SDL_GetTicksNS() + NTSC_VBLANK_NS;
    target = next_vblank_ns;
    for (;;) {
        uint64_t now = SDL_GetTicksNS();
        if (now >= target) break;
        if (target - now > 2000000) SDL_DelayNS(target - now - 1000000);
        else SDL_DelayPrecise(target - now);
    }
    prof_pop();
    ps2_poll_interrupts();
}

/* ---------------- semaphores ---------------- */
#define MAX_SEMA 64
static struct { int used, count, max; } sema[MAX_SEMA];

int CreateSema(struct SemaParam* p)
{
    for (int i = 1; i < MAX_SEMA; i++) {
        if (!sema[i].used) {
            sema[i].used = 1; sema[i].count = p->initCount; sema[i].max = p->maxCount;
            return i;
        }
    }
    return -1;
}
int DeleteSema(int id)  { if (id > 0 && id < MAX_SEMA) sema[id].used = 0; return id; }
int SignalSema(int id)  { if (id > 0 && id < MAX_SEMA) sema[id].count++; return id; }
int iSignalSema(int id) { return SignalSema(id); }
int PollSema(int id)
{
    if (id <= 0 || id >= MAX_SEMA || sema[id].count <= 0) return -1;
    sema[id].count--;
    return id;
}
int WaitSema(int id)
{
    if (id <= 0 || id >= MAX_SEMA) return -1;
    if (sema[id].count <= 0) {
        /* Single-threaded host: whoever signals this has already run (IOP RPCs complete
           synchronously). Reaching here means a missing signal - report, don't deadlock. */
        static int warned;
        if (!warned++) fprintf(stderr, "kernel: WaitSema(%d) would block forever; continuing\n", id);
        return id;
    }
    sema[id].count--;
    return id;
}

/* ---------------- threads / alarms ---------------- */
static int wakeups;
static struct { int active; uint64_t due_ns; void (*cb)(int, unsigned short, void*); void* arg; unsigned short time; } alarm_slot;

int GetThreadId(void) { return 1; }
int ChangeThreadPriority(int tid, int prio) { (void)tid; (void)prio; return 0; }
int RotateThreadReadyQueue(int prio) { (void)prio; return 0; }
int WakeupThread(int tid)  { (void)tid; wakeups++; return 0; }
int iWakeupThread(int tid) { return WakeupThread(tid); }

int SetAlarm(unsigned short time, void (*cb)(int, unsigned short, void*), void* arg)
{
    alarm_slot.active = 1;
    alarm_slot.due_ns = SDL_GetTicksNS() + (uint64_t)time * 1000000000ull / NTSC_HSYNC_HZ;
    alarm_slot.cb = cb; alarm_slot.arg = arg; alarm_slot.time = time;
    return 1;
}

int SleepThread(void)
{
    if (wakeups > 0) { wakeups--; return 0; }
    if (alarm_slot.active) {
        uint64_t now = SDL_GetTicksNS();
        if (alarm_slot.due_ns > now) SDL_DelayPrecise(alarm_slot.due_ns - now);
        alarm_slot.active = 0;
        alarm_slot.cb(1, alarm_slot.time, alarm_slot.arg);
        if (wakeups > 0) wakeups--;
        ps2_poll_interrupts();
        return 0;
    }
    fprintf(stderr, "kernel: SleepThread with nothing to wake it\n");
    return 0;
}

/* ---------------- misc ---------------- */
void FlushCache(int op)  { (void)op; }
void iFlushCache(int op) { (void)op; }
int DIntr(void) { return 1; }
int EIntr(void) { return 1; }

/* The engine is quiescent at njUserMain's completed-tick boundary. Preserve
   emulated state, not SDL's clock or any host-thread objects. */
typedef struct {
    uint32_t magic, version, bytes;
    uint64_t vblank;
    __typeof__(intc) interrupts;
    __typeof__(sema) semaphores;
    int wake, graph_field;
    Ps2FakeRegs regs;
    unsigned char scratch[PS2_SPR_SIZE];
} PlatformSnapshot;
int ps2_snapshot_graph_field(void);
void ps2_snapshot_graph_field_restore(int saved);
int snapshot_platform_capture(SnapshotBlob* out)
{
    PlatformSnapshot* p;
    if (!out || alarm_slot.active) return 0; /* never suspend a native alarm callback */
    p = calloc(1, sizeof(*p));
    if (!p) return 0;
    p->magic = 0x32535041; p->version = 1; p->bytes = sizeof(*p); p->vblank = vblank_counter;
    memcpy(p->interrupts, intc, sizeof(intc)); memcpy(p->semaphores, sema, sizeof(sema));
    p->wake = wakeups; p->graph_field = ps2_snapshot_graph_field();
    memcpy(&p->regs, &ps2_regs, sizeof(ps2_regs));
    memcpy(p->scratch, (const void*)(uintptr_t)PS2_SPR_BASE, PS2_SPR_SIZE);
    out->data = (unsigned char*)p; out->size = sizeof(*p);
    return 1;
}
int snapshot_platform_validate(const SnapshotBlob* in)
{
    const PlatformSnapshot* p;
    if (!in || !in->data || in->size != sizeof(*p)) return 0;
    p = (const PlatformSnapshot*)in->data;
    if (p->magic != 0x32535041 || p->version != 1 || p->bytes != sizeof(*p) ||
        p->wake < 0 || p->graph_field < 0 || p->graph_field > 1) return 0;
    for (unsigned i = 0; i < 16; ++i) {
        if (p->interrupts[i].enabled < 0 || p->interrupts[i].enabled > 1) return 0;
        for (unsigned j = 0; j < MAX_HANDLERS; ++j) {
            uintptr_t fn = (uintptr_t)p->interrupts[i].fn[j];
            if (fn && (fn < 0x30000000u || fn >= 0x40000000u)) return 0;
        }
    }
    for (unsigned i = 0; i < MAX_SEMA; ++i)
        if (p->semaphores[i].used < 0 || p->semaphores[i].used > 1 ||
            p->semaphores[i].count < 0 || p->semaphores[i].max < 0 ||
            p->semaphores[i].count > p->semaphores[i].max) return 0;
    return 1;
}
int snapshot_platform_restore(const SnapshotBlob* in)
{
    const PlatformSnapshot* p;
    if (!snapshot_platform_validate(in)) return 0;
    p = (const PlatformSnapshot*)in->data;
    memcpy(intc, p->interrupts, sizeof(intc)); memcpy(sema, p->semaphores, sizeof(sema));
    vblank_counter = p->vblank; wakeups = p->wake; memset(&alarm_slot, 0, sizeof(alarm_slot));
    memcpy(&ps2_regs, &p->regs, sizeof(ps2_regs));
    memcpy((void*)(uintptr_t)PS2_SPR_BASE, p->scratch, PS2_SPR_SIZE);
    ps2_snapshot_graph_field_restore(p->graph_field);
    next_vblank_ns = SDL_GetTicksNS() + NTSC_VBLANK_NS;
    return 1;
}
