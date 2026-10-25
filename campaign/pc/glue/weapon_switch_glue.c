/* Shared fixed-camera/TPS shortcuts. Keep requests for just this simulation tick:
   menus/scripts must never defer an equipment change until control returns. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/sub1.h>
#include <ps2/veronica/prog/item.h>
#include <stdio.h>
#include <stdlib.h>

unsigned pad_weapon_switch(void);
int pc_in_play(const BH_PWORK* p, int scope);
int knife_switch_busy(void);
static unsigned previous, request;

static int allowed(void)
{
    return sys && plp && sys->ply_id < 4 && pc_in_play(plp, 0) &&
        (sys->sp_flg & 1) && plp->hp > 0 && !knife_switch_busy() &&
        !(plp->flg & 0x4000000) && !(plp->stflg & 0x10400) && !plp->psh_ct &&
        plp->mode1 == 0 && ((plp->mode2 <= 11 && !(plp->flg & 0x10000)) || plp->mode2 == 13);
}

/* bhSetPad runs even when bhControlPlayer is paused. Overwrite, never accumulate. */
void weapon_switch_pad(void)
{
    unsigned held = pad_weapon_switch();
    static int trace = -1;
    if (trace < 0) trace = getenv("RECVX_WEAPON_TRACE") != NULL;
    request = held & ~previous;
    previous = held;
    if (trace && request && sys && plp)
        printf("weapon input: %x allowed %d mode %d/%d/%d/%d flg %x status %x push %d sp %x st %x gm %x cb %x hp %d\n",
            request, allowed(), plp->mode0, plp->mode1, plp->mode2, plp->mode3,
            plp->flg, plp->stflg, plp->psh_ct, sys->sp_flg, sys->st_flg, sys->gm_flg, sys->cb_flg, plp->hp);
    if (!allowed()) request = 0;
}

/* After the native mode handler and reload: re-check ownership/animation, then use
   the inventory's asynchronous model/motion loading path without opening its UI. */
void weapon_switch_player(void)
{
    unsigned pressed = request;
    unsigned* iv;
    int slots[10], count = 0, last, current = -1, target = -1;
    request = 0;
    if (!pressed || !allowed()) return;
    iv = &sys->itm[sys->ply_id * 16];
    last = sys->gm_flg & 0x8000000 ? 11 : 9;
    for (int i = 2; i <= last; ++i) {
        /* Ammo-kind and unlimited flags are separate from the eleven-bit ID. */
        unsigned id = (iv[i] >> 16) & 0x7ff;
        if (id >= 156) continue;
        if (itemdata[id].type & 1) {
            if (iv[0] == (unsigned)i) current = count;
            slots[count++] = i;
        }
        /* Two-cell items still occupy one packed inventory word (ItemSort).
           The following word can hold a different weapon; never skip it. */
    }
    if (!count) return;
    /* Deterministic simultaneous presses: first direct ordinal wins; opposite
       cycling directions cancel. An absent direct ordinal never falls back. */
    if (pressed & 0x3c) {
        for (int n = 0; n < 4; ++n) if (pressed & (4u << n)) { target = n; break; }
    } else if ((pressed & 3) == 2) target = (current + 1) % count;
    else if ((pressed & 3) == 1) target = current < 0 ? count - 1 : (current + count - 1) % count;
    if (target < 0 || target >= count || iv[0] == (unsigned)slots[target]) return;
    swork.pip = iv;
    printf("weapon switch: slot %d -> %d, item %u\n", (int)iv[0], slots[target], (iv[slots[target]] >> 16) & 0x7ff);
    WeaponSet(slots[target], slots[target]);
}
