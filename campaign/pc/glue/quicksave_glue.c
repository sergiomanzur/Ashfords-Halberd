/* Full room snapshots at a stable end-of-frame boundary. Native room opening
   tasks never run during quickload; saved game and platform state resume directly. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "../src/host/quicksave.h"
#include "../src/host/room_snapshot.h"
#include <stdio.h>
#include <math.h>

unsigned pad_quicksave_actions(void);
int pc_in_play(const BH_PWORK* p, int scope);
int knife_switch_busy(void);
static unsigned previous, request;
static int restoring;

static int blocked(void)
{
    if (!sys || plp != &ply || restoring || room_snapshot_busy() || !(sys->tk_flg & 0x80) ||
        !pc_in_play(plp, 0) || !(sys->sp_flg & 1) || plp->hp <= 0 ||
        !plp->exp0 || !plp->exp1 || knife_switch_busy() ||
        (plp->flg & 0x4000000) || (plp->stflg & 0x10400) || plp->psh_ct ||
        (sys->st_flg & 0x200) || (sys->cb_flg & 0x600000) || plp->mode1 != 0 ||
        !((plp->mode2 <= 11 && !(plp->flg & 0x10000)) || plp->mode2 == 13)) return QS_BLOCKED;
    if (sys->ply_id < 0 || sys->ply_id >= 4 || sys->gm_mode < 0 || sys->gm_mode > 2 ||
        sys->stg_no < 0 || sys->stg_no > 9 || sys->ewk_n < 0 || sys->ewk_n > 128 ||
        sys->rom_no < 0 || sys->rcase < 0 || sys->rcase > 15 || sys->pos_no < 0 ||
        sys->save_ct < 0 || sys->save_ct > 99 || sys->retry_ct < 0 || sys->retry_ct > 99 ||
        plp->wpnr_no < 0 || plp->wpnr_no >= 23 || !(fabsf(plp->px) < 1000000) ||
        !(fabsf(plp->py) < 1000000) || !(fabsf(plp->pz) < 1000000)) return QS_BAD_STATE;
    if (sys->evt_tim || sys->evt_tmd) return QS_TIMER;
    for (int i = 0; i < sys->ewk_n; ++i) {
        const BH_PWORK* e = &ene[i];
        /* Native bhJumpEnemy dispatch: real hostile handlers versus bhEne00
           dummies / bhSubpl actors. Zombies at zero HP are still alive, and
           negative HP can remain in an unfinished death animation. */
        int hostile = (e->id >= 1 && e->id <= 30) || (e->id >= 53 && e->id <= 56) || e->id == 71;
        if ((e->flg & 1) && (e->flg & 0x20) && !(e->flg & 2) &&
            !(e->stflg & 0x1000000) && (e->hp > 0 || hostile)) return QS_COMBAT;
    }
    return QS_NONE;
}
void quicksave_pad(void)
{
    unsigned held = pad_quicksave_actions(), pressed = held & ~previous;
    int reason;
    previous = held;
    request = 0; /* overwrite even if a paused game never consumed the last sample */
    if (!pressed) return;
    if ((held & 3) == 3) { quicksave_notice_set(QS_BOTH); return; }
    reason = blocked();
    if (reason) { quicksave_notice_set(reason); printf("quicksave: request %u blocked reason %d\n", pressed, reason); return; }
    request = pressed & 3;
}
static void trace(const char* action)
{
    printf("quicksave: %s room %d/%d case %d position %.5f %.5f %.5f hp %d inventory %08x saves %d retries %d flags %08x\n",
        action, sys->stg_no, sys->rom_no, sys->rcase, plp->px, plp->py, plp->pz,
        plp->hp, sys->itm[sys->ply_id * 16 + 2], sys->save_ct, sys->retry_ct, sys->rm_flg);
}
void quicksave_frame(void)
{
    unsigned action = request;
    int result, reason, message;
    request = 0;
    if (!action) return;
    reason = blocked();
    if (reason) { quicksave_notice_set(reason); return; }
    if (!room_snapshot_available()) { quicksave_notice_set(QS_UNAVAILABLE); return; }
    if (action == 1) {
        result = room_snapshot_capture(quicksave_path());
        message = result == SNAP_OK ? QS_SAVED : result == SNAP_WRONG_BUILD ? QS_WRONG_BUILD : QS_WRITE_FAILED;
    } else {
        printf("quicksave: full reload begin (current hp %d inventory %08x)\n",
            plp->hp, sys->itm[sys->ply_id * 16 + 2]);
        restoring = 1;
        result = room_snapshot_restore(quicksave_path());
        restoring = 0;
        message = result == SNAP_OK ? QS_RESTORED : result == SNAP_MISSING ? QS_MISSING :
            result == SNAP_WRONG_BUILD ? QS_WRONG_BUILD : result == SNAP_INVALID ? QS_CORRUPT : QS_READ_FAILED;
    }
    quicksave_notice_set(message);
    if (result == SNAP_OK) trace(action == 1 ? "saved" : "restored");
}
