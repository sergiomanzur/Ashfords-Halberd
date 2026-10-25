/* Real quicksave frame/gates; backend fixture preserves full live room state. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "../src/host/quicksave.h"
#include "../src/host/room_snapshot.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static SYS_WORK state;
SYS_WORK* sys = &state;
BH_PWORK ply, ene[128];
BH_PWORK* plp = &ply;
static unsigned char checkpoint[sizeof(SAVEFILE)];
unsigned char* freemem = checkpoint;
typedef struct { SYS_WORK sys; BH_PWORK player, enemies[128]; } RoomImage;
static RoomImage disk, golden;
static unsigned held;
static int reads, writes, pops, notice, knife, read_result, write_result, failures, checks;
static int available = 1, busy;
unsigned pad_quicksave_actions(void) { return held; }
int knife_switch_busy(void) { return knife; }
const char* quicksave_path(void) { return "test_quicksave.ahs"; }
void quicksave_notice_set(int n) { notice = n; }
int room_snapshot_available(void) { return available; }
int room_snapshot_busy(void) { return busy; }
int room_snapshot_capture(const char* path) {
    (void)path; writes++;
    if (write_result == SNAP_OK) { disk.sys = *sys; disk.player = *plp; memcpy(disk.enemies, ene, sizeof(ene)); }
    return write_result;
}
int room_snapshot_restore(const char* path) {
    (void)path; reads++;
    if (read_result == SNAP_OK) { *sys = disk.sys; *plp = disk.player; memcpy(ene, disk.enemies, sizeof(ene)); }
    return read_result;
}
void bhPopGameData(void) { pops++; } /* Any accidental native reload fails the assertions. */
#include "quick_in_play_test.inc"
static void check(int yes, const char* text)
{
    checks++;
    if (!yes) { fprintf(stderr, "FAIL %s\n", text); failures++; }
}
static void control(void)
{
    sys->sp_flg = 0x41; sys->tk_flg = 0x80;
    sys->st_flg = sys->ss_flg = sys->cb_flg = sys->ts_flg = 0;
    sys->gm_flg = 2; sys->mn_mode0 = sys->mn_md0 = 0;
    sys->evt_tim = sys->evt_tmd = 0;
    ply.mode0 = 1; ply.mode1 = ply.mode2 = ply.mode3 = 0;
    ply.flg = 0x20000; ply.stflg = 0; ply.psh_ct = 0;
    ply.mnwP = (void*)&state; ply.mlwP = (void*)&state;
    ply.exp0 = ply.exp1 = (void*)&state;
    ply.hp = 130; knife = 0;
}
static void tick(unsigned mask) { held = mask; quicksave_pad(); quicksave_frame(); }
static void reset(void)
{
    memset(&state, 0, sizeof(state)); memset(&ply, 0, sizeof(ply)); memset(ene, 0, sizeof(ene));
    memset(checkpoint, 0xEE, sizeof(checkpoint));
    control(); sys->version = 10; sys->save_ct = 7; sys->retry_ct = 4;
    sys->itm[0] = 2; sys->itm[2] = (9u << 16) | 7;
    ply.px = 14.5f; ply.py = 2; ply.pz = -27; ply.ay = 1200; ply.flr_no = 2; ply.wpnr_no = 4;
    ply.stflg = 0x40280000;
    read_result = write_result = SNAP_OK; available = 1; busy = 0;
    tick(0); reads = writes = pops = notice = 0;
}
int main(void)
{
    reset(); sys->rm_flg = 0x13572468; sys->ev_flg[3] = 0x00400000; sys->loop_ct = 773; tick(1);
    check(writes == 1 && notice == QS_SAVED, "normal control writes room snapshot"); golden = disk;
    check(disk.player.px == ply.px && disk.player.py == ply.py && disk.player.pz == ply.pz, "live xyz serialized");
    check(disk.player.ay == 1200 && disk.player.flr_no == 2 && disk.player.hp == 130 && disk.player.wpnr_no == 4, "live floor angle hp weapon serialized");
    check(disk.sys.save_ct == 7 && disk.sys.retry_ct == 4 && sys->save_ct == 7 && sys->retry_ct == 4, "save stats preserved");
    check(disk.sys.itm[2] == ((9u << 16) | 7) && checkpoint[0] == 0xEE, "inventory and native continue checkpoint preserved");
    check(disk.sys.rm_flg == 0x13572468 && disk.sys.ev_flg[3] == 0x00400000 && disk.sys.loop_ct == 773, "full room flags and scheduler state serialized");
    tick(1); check(writes == 1, "held input does not repeat");
    for (int gate = 0; gate < 28; ++gate) {
        reset();
        switch (gate) {
        case 0: sys->sp_flg &= ~1; break;
        case 1: sys->sp_flg &= ~0x40; break;
        case 2: sys->st_flg |= 1; break;
        case 3: sys->st_flg |= 0x800000; break;
        case 4: sys->gm_flg |= 1; break;
        case 5: sys->gm_flg |= 8; break;
        case 6: sys->gm_flg |= 0x40; break;
        case 7: sys->cb_flg |= 1; break;
        case 8: sys->cb_flg |= 4; break;
        case 9: sys->ts_flg |= 0x80; break;
        case 10: sys->mn_mode0 = 1; break;
        case 11: sys->mn_md0 = 1; break;
        case 12: ply.flg |= 0x4000000; break;
        case 13: ply.flg |= 0x10000; break;
        case 14: ply.stflg |= 0x10400; break;
        case 15: ply.mode0 = 2; break;
        case 16: ply.mode1 = 1; ply.mode2 = 70; break;
        case 17: ply.mode2 = 12; break;
        case 18: ply.psh_ct = 1; break;
        case 19: ply.hp = 0; break;
        case 20: knife = 1; break;
        case 21: sys->st_flg |= 0x200; break;
        case 22: sys->cb_flg |= 0x200000; break;
        case 23: sys->cb_flg |= 0x400000; break;
        case 24: sys->evt_tim = 120; break;
        case 25: sys->evt_tmd = 1; break;
        case 26: sys->ewk_n = 1; ene[0].flg = 0x21; ene[0].hp = 100; break;
        case 27: sys->tk_flg = 0; break;
        }
        tick(1); check(!writes && !reads && !pops, "unsafe save blocked before IO");
        tick(0); tick(2); check(!reads && !pops, "unsafe load blocked before IO");
        control(); sys->ewk_n = 0;
        tick(2); check(!reads && !pops, "unsafe held request never queues");
        tick(0); tick(1); check(writes == 1, "fresh safe press succeeds");
    }
    reset(); sys->ewk_n = 1; ene[0].flg = 1; ene[0].hp = 100;
    tick(1); check(writes == 1, "non-targetable room actor is not combat");
    reset(); sys->ewk_n = 1; ene[0].id = 1; ene[0].flg = 0x21; ene[0].hp = 0;
    tick(1); check(!writes, "zero-health targetable zombie is still unsafe");
    reset(); sys->ewk_n = 1; ene[0].id = 1; ene[0].flg = 0x21; ene[0].hp = -1;
    tick(1); check(!writes, "hostile death animation must finish before saving");
    reset(); sys->ewk_n = 1; ene[0].id = 0; ene[0].flg = 0x21; ene[0].hp = 0;
    tick(1); check(writes == 1, "zero-health static dummy is not a dying hostile");
    reset(); tick(3); check(!writes && !reads && notice == QS_BOTH, "simultaneous requests cancel");
    reset(); held = 1; quicksave_pad(); ply.mode1 = 1; quicksave_frame();
    check(!writes, "state rechecked at frame boundary");
    reset(); held = 1; quicksave_pad(); held = 0; quicksave_pad(); quicksave_frame();
    check(!writes, "requests expire if frame consumer was skipped");

    reset(); write_result = SNAP_IO_ERROR; tick(1); check(notice == QS_WRITE_FAILED, "write failure feedback");
    static const int errors[] = { SNAP_IO_ERROR, SNAP_INVALID, SNAP_MISSING, SNAP_WRONG_BUILD };
    static const int messages[] = { QS_READ_FAILED, QS_CORRUPT, QS_MISSING, QS_WRONG_BUILD };
    for (unsigned e = 0; e < sizeof(errors) / sizeof(errors[0]); ++e) {
        reset(); read_result = errors[e]; tick(2);
        check(!pops && ply.hp == 130 && checkpoint[0] == 0xEE, "bad or missing file leaves current game intact");
        if (messages[e] >= 0) check(notice == messages[e], "snapshot error maps to actionable status");
    }
    reset(); available = 0; tick(1); check(!writes && notice == QS_UNAVAILABLE, "unavailable snapshot build blocks save before IO");
    tick(0); tick(2); check(!reads && notice == QS_UNAVAILABLE, "unavailable snapshot build blocks load before IO");
    reset(); busy = 1; tick(1); check(!writes && notice == QS_BLOCKED, "snapshot transaction in progress blocks new request");
    reset(); ply.px = NAN; tick(1); check(!writes && notice == QS_BAD_STATE, "nonfinite live position rejected before capture");
    reset(); sys->rcase = 16; tick(1); check(!writes && notice == QS_BAD_STATE, "invalid room case rejected before capture");
    reset(); ply.wpnr_no = 23; tick(1); check(!writes && notice == QS_BAD_STATE, "invalid live weapon rejected before capture");
    reset(); ply.wpnr_no = -1; tick(1); check(!writes && notice == QS_BAD_STATE, "negative live weapon rejected before capture");
    reset(); disk = golden; disk.sys.retry_ct = 99;
    sys->rm_flg = 0; sys->ev_flg[3] = 0; sys->loop_ct = 999; ply.px = 801; ply.hp = 50; sys->itm[2] = 0;
    tick(2);
    check(reads == 1 && !pops && notice == QS_RESTORED && sys->retry_ct == 99, "direct room restore preserves saturated retry count");
    check(sys->save_ct == 7 && ply.px == golden.player.px && ply.py == golden.player.py && ply.pz == golden.player.pz, "saved live player restored directly");
    check(sys->rm_flg == 0x13572468 && sys->ev_flg[3] == 0x00400000 && sys->loop_ct == 773, "room event flags and scheduler restored without opening replay");
    check(ply.hp == 130 && ply.wpnr_no == 4 && sys->itm[2] == golden.sys.itm[2], "player health weapon and inventory restored");
    check(checkpoint[0] == 0xEE, "native continue checkpoint remains untouched on load");
    tick(2); check(reads == 1, "held load never repeats after room restore");
    tick(0); control(); disk = golden; tick(2);
    check(reads == 2 && !pops && sys->retry_ct == 4 && notice == QS_RESTORED, "ordinary retry count remains unchanged on direct load");
    fprintf(stderr, "%d quicksave checks, %d failures\n", checks, failures);
    return !!failures;
}
