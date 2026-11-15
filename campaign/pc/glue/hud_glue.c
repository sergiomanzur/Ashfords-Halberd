/* HUD: each frame what the overlay draws (overlay.cpp hud_draw), published by the game thread.
   Nothing of the game is changed. Hidden in menus, events, scripted scenes and the Battle Game's
   non-play states; Auto shows it with the over-the-shoulder camera, On also with fixed cameras. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/player.h>
#include <ps2/veronica/prog/sub1.h>
#include "../src/host/tps.h"
#include <stdio.h>
#include <string.h>

TpsHud g_hud;
void host_log(const char* fmt, ...);
int pc_in_game(void);      /* options_glue.c */
int pc_in_play(const BH_PWORK* p, int scope);
short bhCheckBullet(void);
extern int tps_cam_active;

void hud_frame(void)
{
    unsigned int* inv;
    int hp_max, bn;
    if (!sys || !plp) return;
    inv = &sys->itm[sys->ply_id * 16];
    g_hud.on = g_tps.hud != 2 && pc_in_game() && plp && plp->hp >= 0 && sys->gm_mode != 3 &&
               (g_tps.hud == 1 || (g_tps.on && tps_cam_active)) &&
               (plp->mode0 == 1 || plp->mode0 == 2 || plp->mode0 == 4 || plp->mode0 == 5) &&
               !*(int*)&sys->mn_mode0 && !(sys->cb_flg & 0x5);
    if (!g_hud.on) return;
    hp_max = sys->gm_mode == 2 ? 320 : 160;
    g_hud.hp = plp->hp < 0 ? 0 : plp->hp;
    g_hud.hp_max = hp_max;
    g_hud.cond = (plp->stflg & 0x280000) ? HUD_POISON
               : g_hud.hp * 4 >= hp_max * 3 ? HUD_FINE
               : g_hud.hp * 8 >= hp_max * 3 ? HUD_CAUTION
               : g_hud.hp * 16 >= hp_max * 3 ? HUD_CAUTION2 : HUD_DANGER;
    g_hud.weapon = inv[0] < 16 && (inv[inv[0]] >> 16) ? (int)((inv[inv[0]] >> 16) & 0xFF) : 0;
    bn = bhCheckBullet();
    g_hud.rounds = bn < 0 ? HUD_INF : bn;
    g_hud.spare = HUD_NONE;   /* the bag's mixes are not enumerated here; the loaded rounds are shown */
    g_hud.left = g_tps.hud_side == 1 || (g_tps.hud_side == 0 && !g_tps.left && !tps_cam_active);
}
