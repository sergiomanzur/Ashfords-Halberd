/* Offline BioRand pack gameplay hooks (pc/src/host/biorand.h): rifle ammo stacking, save-word
 * stamping & loading, Nosferatu poison cleanup when doors omit 9/15, ItemBoxChange key-item
 * relaxation, ArmsItemChange unfound weapon guard, and scene cast shot arms-bank sound fix.
 * Strictly offline single-player, game thread.
 */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/item.h>
#include <ps2/veronica/prog/flag.h>
#include <ps2/veronica/prog/player.h>
#include <ps2/veronica/prog/sdfunc.h>
#include "../src/ps2_platform.h"
#include "../src/host/biorand.h"
#include "../src/host/savestore.h"
#include "../src/host/script_rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);
int pc_in_game(void);
static void shot_frame(void);
void spoiler_tick(void);

/* Two unused save-region words (AllItemInit clears them):
   BR_W = 'B'<<24 | seed (0xFFFFF unknown) while BioRand is on, else 0; BR_W + 1 = pack id. */
#define BR_W 288
#define BR_ON 0x42000000u
#define BR_OFFSET ((int)((char*)&((SYS_WORK*)0)->itm[BR_W] - (char*)&((SYS_WORK*)0)->version))

static void words(unsigned int* w)
{
    int on = biorand_active(), seed = biorand_seed();
    w[0] = on ? BR_ON | (seed >= 0 ? (unsigned int)seed : 0xFFFFFu) : 0;
    w[1] = on ? biorand_id() : 0;
}

void pc_biorand_stamp(void)
{
    if (sys) words(&sys->itm[BR_W]);
}

/* system.c case 5 (after AllItemInit): activate BioRand for a new campaign when enabled */
void pc_quick_start(void)
{
    int want = sys && sys->gm_mode != 3 && biorand_pack() && g_config.biorand;
    biorand_game(want);
    pc_biorand_stamp();
}

/* save_glue.c: align active BioRand state with loaded save words */
void pc_biorand_save_apply(const void* save)
{
    unsigned int w[2];
    int seed;
    memcpy(w, (const char*)save + BR_OFFSET, sizeof(w));
    if ((w[0] & 0xFF000000u) != BR_ON) {
        biorand_game(0);
        return;
    }
    seed = (int)(w[0] & 0xFFFFF);
    if (!biorand_pack()) {
        biorand_game(0);
        host_log("biorand: loaded save belongs to BioRand (seed %d), but no pack is in mods/biorand; playing without BioRand\n",
                 seed == 0xFFFFF ? -1 : seed);
        return;
    }
    biorand_game(1);
    if (w[1] != biorand_id())
        host_log("biorand: loaded save (seed %d, id %08x) differs from installed pack (seed %d, id %08x)\n",
                 seed == 0xFFFFF ? -1 : seed, w[1], biorand_seed(), biorand_id());
}

/* sub1.c ItemBoxChange: BioRand places key items anywhere, so Claire may box Security Card/File/Crystal
   and retrieve key items from the box in 9/26 (Antarctic mansion study). */
int pc_biorand_box_any(void) { return biorand_active(); }

void biorand_frame(void)
{
    unsigned short on = biorand_rifle_stack() ? 0x200 : 0;
    itemdata[40].type = (itemdata[40].type & ~0x200) | on;
    itemdata[42].type = (itemdata[42].type & ~0x200) | on;
    /* Nosferatu's poison (stflg 0x200000) ends with him (ed_flg 205) in BioRand so door-randomizer
       seeds that skip Chris's Serum room 9/15 do not leave Claire poisoned permanently. */
    if (pc_in_game() && biorand_active() && bhCkFlg(sys->ed_flg, 205) && ((plp->stflg | sys->ply_stflg[0]) & 0x200000)) {
        plp->stflg &= ~0x200000;
        sys->ply_stflg[0] &= ~0x200000;
        bhCrFlg(sys->ev_flg, 307);
        host_log("biorand: Nosferatu is dead: Claire's poison cured (ev_flg 307 cleared)\n");
    }
    shot_frame();
    spoiler_tick();
}

/* ---------------------------------------------------------------- cast member shot sound in scenes */
extern BH_SCEWORK bhEtask[16];
static int shot_w = -1, shot_lvl = -1, shot_bank;
static const void* shot_evtp;

static int arms_bank(int w) { return w >= 0 && w < 23 ? WpnTab[w].snd_wpno : -1; }

static unsigned int scripts(void)
{
    const unsigned int* tab = (const unsigned int*)rom->evtp;
    unsigned int first = tab[0], k;
    for (k = 0; k * 4 < first && k < 1024; k++) if (tab[k] && tab[k] < first) first = tab[k];
    return first / 4;
}

static int find_op(unsigned int o, int op, int sub)
{
    const unsigned char* d = (const unsigned char*)rom->evtp;
    const unsigned int* tab = (const unsigned int*)rom->evtp;
    unsigned int lim = 0, k, ns = scripts();
    int n;
    for (k = 0; k < ns; k++) if (tab[k] > o && (!lim || tab[k] < lim)) lim = tab[k];
    for (k = 0; k < 8192 && (!lim || o < lim) && (n = pc_insn_len_raw(d + o)) > 0; k++, o += (unsigned int)n) {
        if (d[o] == op && (sub < 0 || d[o + 1] == sub)) return (int)o;
        if (!lim && (d[o] == 0 || d[o] == 255)) break;
    }
    return -1;
}

/* event.c Event_init: task level runs the event at p */
void pc_cast_shot_event(int level, const unsigned char* p)
{
    const unsigned int* tab;
    unsigned int k, ns;
    int at;
    if (level < 0 || level >= 16 || !rom || !rom->evtp) return;
    if (level == shot_lvl) shot_w = -1;
    if ((at = find_op((unsigned int)(p - (const unsigned char*)rom->evtp), 73, 1)) < 0) return;   /* WeaponSeCall 1 */
    at = ((const unsigned char*)rom->evtp)[at + 5];
    if (at >= 23 || !WpnTab[at].seno0) return;
    tab = (const unsigned int*)rom->evtp;
    for (k = 0, ns = scripts(); k < ns; k++) if (tab[k] && find_op(tab[k], 39, -1) >= 0) return;   /* ArmsItemChange */
    shot_w = at; shot_lvl = level; shot_evtp = rom->evtp;
    host_log("cast shot: task level %d fires weapon %d, the room sets no weapon: its arms bank %d for the scene\n",
             level, at, arms_bank(at));
}

/* event.c bhWeaponSeCall 1: sound of the cast's shot with weapon w */
int pc_cast_shot_se(int w)
{
    if (shot_bank && shot_w >= 0 && w >= 0 && w < 23 && arms_bank(w) == arms_bank(shot_w)) return WpnTab[w].seno0;
    return WpnTab[plp->wpnr_no].seno0;
}

/* event.c bhArmsItemChange: under BioRand, do not equip a weapon the player does not hold */
int pc_biorand_arms_skip(int w)
{
    static const unsigned char item[23] = { 0, 55, 8, 5, 9, 10, 32, 33, 34, 142, 7, 4, 2, 3, 6, 6, 6, 6, 11, 7, 1, 0, 0 };
    const unsigned int* bag;
    int i, id;
    if (w <= 0 || w >= 23 || !item[w] || !sys || !biorand_active()) return 0;
    bag = &sys->itm[sys->ply_id * 16];
    for (i = 1; i < 12; i++)
        if ((id = (bag[i] >> 16) & 0xFF) == item[w] || (w == 3 && id == 131)) return 0;
    host_log("biorand: the script's weapon %d is not in the bag: the hand keeps weapon %d\n", w, plp ? plp->wpnr_no : -1);
    return 1;
}

static void shot_frame(void)
{
    int b;
    if (shot_w >= 0 && (!pc_in_game() || !rom || rom->evtp != shot_evtp || !bhEtask[shot_lvl].status)) shot_w = -1;
    if (!pc_in_game() || !plp || CheckTransEndSoundBank()) return;
    if (shot_w >= 0 && !shot_bank && arms_bank(shot_w) != arms_bank(plp->wpnr_no)) b = arms_bank(shot_w), shot_bank = 1;
    else if (shot_w < 0 && shot_bank) b = arms_bank(plp->wpnr_no), shot_bank = 0;
    else return;
    host_log("cast shot: arms bank %d (%s)\n", b, shot_bank ? "the scene's weapon" : "the player's again");
    if (b >= 0) RequestArmsSoundBank(b);
}
