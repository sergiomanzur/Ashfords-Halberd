#include "room_snapshot.h"
#include "game_rng.h"
#include "tps.h"
#include "enemies.h"
#include "actors.h"
#include "cheat.h"
#include "cheat_room_snapshot.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t version, bytes;
    GameRngState rng;
    int tps_on, cam_active, aiming, pad_play, kb_play;
    EnemyOpts enemies;
    int appear_anim, appear_face, appear_cutscene;
    int health, ammo, freeze_time, noclip, unlock;
    CheatRoomSnapshot cheat_room;
} SnapshotPreferences;
int snapshot_preferences_capture(SnapshotBlob* out)
{
    SnapshotPreferences* p;
    if (!out || !(p = calloc(1, sizeof(*p)))) return 0;
    p->version = 2; p->bytes = sizeof(*p); game_rng_capture(&p->rng);
    if (!cheat_room_snapshot_capture(&p->cheat_room)) { free(p); return 0; }
    p->tps_on = g_tps.on; p->cam_active = g_tps.cam_active; p->aiming = g_tps.aiming;
    p->pad_play = g_tps.pad_play; p->kb_play = g_tps.kb_play; p->enemies = g_enemy_opts;
    p->appear_anim = g_appear_anim; p->appear_face = g_appear_face; p->appear_cutscene = g_appear_cutscene;
    p->health = g_cheat.inf_health; p->ammo = g_cheat.inf_ammo; p->freeze_time = g_cheat.freeze_time;
    p->noclip = g_cheat.noclip; p->unlock = g_cheat.unlock;
    out->data = (unsigned char*)p; out->size = sizeof(*p); return 1;
}
int snapshot_preferences_validate(const SnapshotBlob* in)
{
    const SnapshotPreferences* p;
    if (!in || !in->data || in->size != sizeof(*p)) return 0;
    p = (const SnapshotPreferences*)in->data;
    return p->version == 2 && p->bytes == sizeof(*p) && game_rng_validate(&p->rng) && cheat_room_snapshot_validate(&p->cheat_room) &&
        p->tps_on >= 0 && p->tps_on <= 2 &&
        p->enemies.extra >= 0 && p->enemies.extra <= 3 && p->enemies.place >= 0 && p->enemies.place <= 1 &&
        p->enemies.rand >= 0 && p->enemies.rand <= 1 && p->enemies.variety >= 0 && p->enemies.variety <= 1 &&
        p->enemies.events >= 0 && p->enemies.events <= 1 && p->noclip >= 0 && p->noclip <= 2 &&
        p->health >= 0 && p->health <= 1 && p->ammo >= 0 && p->ammo <= 1 && p->freeze_time >= 0 && p->freeze_time <= 1 &&
        p->unlock >= 0 && p->unlock <= 1 && p->appear_anim >= 0 && p->appear_anim <= 1 &&
        p->appear_face >= 0 && p->appear_face <= 1 && p->appear_cutscene >= 0 && p->appear_cutscene <= 1;
}
int snapshot_preferences_restore(const SnapshotBlob* in)
{
    const SnapshotPreferences* p;
    if (!snapshot_preferences_validate(in)) return 0;
    p = (const SnapshotPreferences*)in->data;
    game_rng_restore(&p->rng);
    cheat_room_snapshot_restore(&p->cheat_room);
    g_tps.on = p->tps_on; g_tps.cam_active = p->cam_active; g_tps.aiming = p->aiming;
    g_tps.pad_play = p->pad_play; g_tps.kb_play = p->kb_play; g_enemy_opts = p->enemies;
    g_appear_anim = p->appear_anim; g_appear_face = p->appear_face; g_appear_cutscene = p->appear_cutscene;
    g_cheat.inf_health = p->health; g_cheat.inf_ammo = p->ammo; g_cheat.freeze_time = p->freeze_time;
    g_cheat.noclip = p->noclip; g_cheat.unlock = p->unlock;
    __atomic_store_n(&g_cheat.request, 0, __ATOMIC_RELEASE);
    return 1;
}
