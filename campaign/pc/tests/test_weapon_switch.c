/* Real glue + retail item table; WeaponSet is a spy for the native async equip. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/sub1.h>
#include <ps2/veronica/prog/item.h>
#include <stdio.h>
#include <string.h>

static SYS_WORK state;
SYS_WORK* sys = &state;
BH_PWORK ply;
BH_PWORK* plp = &ply;
S_WORK swork;
static unsigned held;
static int calls, chosen, knife_busy, failures, cases;
unsigned pad_weapon_switch(void) { return held; }
int knife_switch_busy(void) { return knife_busy; }
int pc_in_play(const BH_PWORK* p, int scope)
{
    (void)scope;
    return p == &ply && p->mnwP && p->mlwP && (sys->sp_flg & 0x40) &&
        !(sys->ss_flg & 0x400000) && !(sys->st_flg & 0x800001) &&
        !(sys->gm_flg & 0x49) && !(sys->cb_flg & 5) && !(sys->ts_flg & 0x80) &&
        !*(int*)&sys->mn_mode0 && !*(int*)&sys->mn_md0 && p->mode0 == 1;
}
void WeaponSet(unsigned slot, unsigned same)
{
    if (same != slot || swork.pip != &sys->itm[sys->ply_id * 16]) failures++;
    calls++; chosen = slot;
    swork.pip[0] = slot;
    *(int*)&sys->mn_mode0 = 3; /* native asynchronous load gates later requests */
}
void weapon_switch_pad(void);
void weapon_switch_player(void);
static void check(int ok, const char* msg)
{
    cases++;
    if (!ok) { if (failures < 25) fprintf(stderr, "FAIL %s\n", msg); failures++; }
}
static unsigned* inv(void) { return &sys->itm[sys->ply_id * 16]; }
static void tick(unsigned mask)
{
    held = mask; weapon_switch_pad(); weapon_switch_player();
}
static void reset(void)
{
    memset(&state, 0, sizeof(state)); memset(&ply, 0, sizeof(ply));
    memset(&swork, 0, sizeof(swork));
    ply.mode0 = 1; ply.mnwP = (void*)&state; ply.mlwP = (void*)&state;
    ply.hp = 100; ply.flg = 0x20000; sys->sp_flg = 0x41;
    knife_busy = calls = chosen = 0;
    tick(0);
    inv()[2] = (8u << 16); /* zero-count knife is still a weapon */
    inv()[4] = (9u << 16) | 7;
    inv()[6] = (43u << 16) | 1; /* key */
    inv()[8] = (32u << 16) | 15;
    inv()[9] = (12u << 16) | 10; /* ammunition */
    inv()[10] = (5u << 16) | 18; /* only available with expanded inventory */
    inv()[0] = 2;
}
int main(void)
{
    reset(); tick(2); check(calls == 1 && chosen == 4, "cycle next skips empty/key/ammo");
    *(int*)&sys->mn_mode0 = 0;
    tick(2); check(calls == 1, "held input never repeats");
    tick(0); tick(1); check(calls == 2 && chosen == 2, "previous wraps");
    reset(); tick(1); check(chosen == 8, "previous wraps first to last");
    reset(); inv()[0] = 0; tick(2); check(chosen == 2, "unequipped next selects first");
    reset(); inv()[0] = 0; tick(1); check(chosen == 8, "unequipped previous selects last");
    for (int n = 0; n < 4; ++n) {
        reset(); tick(4u << n);
        int expected[] = {0,4,8,0};
        check(chosen == expected[n], "direct carried ordinal; equipped/absent no-op");
    }
    reset(); sys->gm_flg |= 0x8000000; tick(32); check(chosen == 10, "expanded fourth weapon");
    reset(); sys->st_flg |= 0x8000000; tick(32); check(!calls, "status flag does not expand inventory");
    reset(); inv()[0] = 8; tick(2); check(chosen == 2, "normal capacity excludes hidden slots");
    reset(); sys->ply_id = 1; inv()[2] = 8u << 16; inv()[4] = 9u << 16; tick(8);
    check(chosen == 4 && sys->itm[0] == 2, "current character inventory only");
    reset(); inv()[2] = 2u << 16; inv()[3] = 8u << 16;
    tick(8); check(chosen == 3, "two-cell weapon is one packed word; following weapon remains selectable");
    reset(); inv()[4] = 9u << 16; inv()[5] = 9u << 16;
    tick(16); check(chosen == 5, "separate carried copies remain selectable");
    for (unsigned id = 0; id < 156; ++id) {
        reset(); memset(inv(), 0, 16 * sizeof(unsigned)); inv()[2] = id << 16;
        tick(4); check(calls == !!(itemdata[id].type & 1), "retail equip flags classify every item");
    }
    reset(); inv()[4] = 0xffff0001; inv()[8] = 0; tick(2); check(!calls, "invalid item id ignored");
    for (int gate = 0; gate < 21; ++gate) {
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
        case 8: sys->ts_flg |= 0x80; break;
        case 9: sys->mn_mode0 = 1; break;
        case 10: sys->mn_md0 = 1; break;
        case 11: ply.flg |= 0x4000000; break;
        case 12: ply.flg |= 0x10000; break;
        case 13: ply.stflg |= 0x10400; break;
        case 14: ply.mode0 = 3; break;
        case 15: ply.mode1 = 1; ply.mode2 = 70; break;
        case 16: ply.mode2 = 12; break;
        case 17: ply.psh_ct = 1; break;
        case 18: ply.hp = 0; break;
        case 19: knife_busy = 1; break;
        case 20: ply.mlwP = NULL; break;
        }
        tick(2); check(!calls, "unsafe press blocked");
        /* Restoring control while the key remains held must not queue it. */
        sys->sp_flg = 0x41; sys->st_flg = sys->gm_flg = sys->cb_flg = sys->ts_flg = 0;
        sys->mn_mode0 = sys->mn_md0 = 0; ply.mode0 = 1; ply.mode1 = ply.mode2 = 0;
        ply.flg = 0x20000; ply.stflg = 0; ply.psh_ct = 0; ply.hp = 100;
        ply.mlwP = (void*)&state; knife_busy = 0;
        tick(2); check(!calls, "blocked press discarded until release");
        tick(0); tick(2); check(calls == 1, "fresh press works after control returns");
    }
    for (int mode = 0; mode < 180; ++mode) {
        reset(); ply.mode2 = mode; tick(2);
        check(calls == (mode <= 11 || mode == 13), "only compatible locomotion modes");
    }
    reset(); held = 2; weapon_switch_pad(); ply.mode1 = 1; ply.mode2 = 70;
    weapon_switch_player(); check(!calls, "reload starting same tick wins");
    reset(); held = 2; weapon_switch_pad(); held = 0; weapon_switch_pad();
    weapon_switch_player(); check(!calls, "requests expire if player update is skipped");
    reset(); tick(3); check(!calls, "opposing cycle directions cancel");
    fprintf(stderr, "%d weapon switch checks, %d failures\n", cases, failures);
    return !!failures;
}
