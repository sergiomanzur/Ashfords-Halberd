/* The cutscene cast & voice-line randomizer in the game (offline single-player).
 * Choices: pc/src/host/cast.c & voices.c.
 *
 * At the end of every room load (enemyopt_glue.c enemy_room_finished) each room record of a story character
 * (cast_character: Alfred, Steve, Rodrigo, ...) or of a creature only a cutscene shows (cast_creature) is given
 * another model (cast_model; a zombie only a cutscene shows, cast_scene_zombie: a creature's, cast_crowd_model),
 * loaded into a display slot of the appearance system (ACTOR_CAST + k, one per model the room shows; the
 * retargeting of actor_glue.c). Claire and Chris are never cast: the doubles of the character played use
 * "You appear as" (actor_doubles), and with Per Character models (g_appear_per), the other one's records show
 * the player's look for that character (look_model).
 */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/face.h>
#include <ps2/veronica/prog/adxwrap.h>
#include <ps2/veronica/prog/sdc.h>
#include <ps2/veronica/prog/sdfunc.h>
#include "../src/host/actors.h"
#include "../src/host/cast.h"
#include "../src/host/enemies.h"
#include "../src/host/fileio.h"
#include "../src/host/script_rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int  snd_xbank_load(int key);
int  snd_xbank_prog_ms(int xb, int tq);
int  adx_length_ms(int ptid, int fid);
int  adx_levels(int ptid, int fid, unsigned char* out, int max);

void host_log(const char* fmt, ...);
int cast_n;                              /* hooks run while > 0 */
static void voice_end(const char* by);   /* "Randomize Voice Lines": the line's end (below) */
static void voice_room(void);
static void plan_want(const EnemyOpts* o);
static int plan_ready(const EnemyOpts* o);
static int plan_find(int room, int ofs, int vid, int chr, const VoiceLine** l);
extern unsigned char* bhScePtr;          /* event.c */
extern BH_SCEWORK* bhCetask;
static struct { const BH_SCEWORK *task, *skip; const unsigned char* end; int rec; } pre;

#define CSLOTS (ACTOR_SLOTS - ACTOR_CAST)
#define RECS 128
static signed char rec_slot[RECS];       /* record -> cast slot, -1: drawn as itself */
static unsigned char rec_id[RECS], rec_ver[RECS], rec_chr[RECS];
static short slot_model[CSLOTS] = { -1, -1, -1, -1, -1, -1 };
_Static_assert(CSLOTS == 6, "slot_model");
#define HIDDEN_MAX 64
static NJS_CNK_OBJECT* hidden[HIDDEN_MAX];
static int hidden_n;
static unsigned char hid_logged[4];
static unsigned char lost_logged[RECS / 8];

/* records whose scene needs their own look */
static const struct { unsigned short room; unsigned char rec; const char* why; } kept[] = {
    { 7030, 1, "Alexia asleep in her ice capsule" },
};

static int look_index(int chr, int own)
{
    int want;
    if (!chr) return -1;
    if (chr >= COOP_CHR_ACTOR) return chr - COOP_CHR_ACTOR;
    want = chr == 5 ? 4 : chr - 1;
    return want == own ? -1 : ACTOR_PART + want;
}

/* Claire's or Chris's records (c 0, 1) in cutscenes where they are not played show the player's
   look for them when Per Character models is on; with Same for Both they stay themselves. */
static int look_model(int c)
{
    int kept_flag;
    if (!g_appear_per) return -1;
    return actor_double_model(look_index(appear_look_of(c), c), c, &kept_flag);
}

static int shown(int slot) { return slot >= 0 && slot < CSLOTS && slot_model[slot] >= 0 && actor_shown(ACTOR_CAST + slot); }

/* record k still holds what was given a model */
static int still(const BH_PWORK* ep, int k) { return (ep->flg & 0x1) && ep->id == rec_id[k] && ep->mdlver == rec_ver[k] && shown(rec_slot[k]); }

static int cast_scene(void) { return sys && ((sys->st_flg & 0x1000000) || (sys->cb_flg & 0x4) || (plp && (plp->stflg & 0x1000000))); }

void cast_release(void)
{
    int k;
    for (k = 0; k < CSLOTS; k++)
        if (slot_model[k] >= 0) { actor_set(ACTOR_CAST + k, -1); slot_model[k] = -1; }
    memset(rec_slot, -1, sizeof(rec_slot));
    cast_n = 0;
    hidden_n = 0;
    memset(hid_logged, 0, sizeof(hid_logged));
    memset(lost_logged, 0, sizeof(lost_logged));
}

static int import_count(const EnemyOpts* o)
{
    int n = __atomic_load_n(&actor_import_n, __ATOMIC_ACQUIRE);
    return o->cast_imports && n > 0 ? n : 0;
}

static int cast_want(const EnemyOpts* o, int room, int k, int id, int ver, int imports, int* c, const char** why)
{
    unsigned scene = o->cast_assign ? (unsigned)room + 1 : 0;
    int j;
    if ((*c = cast_character(id, ver)) < 0) *c = cast_creature(room, k, id);
    if (*c < 0 && cast_scene_zombie(room, k, id)) *c = CAST_CROWD;
    if (*c < 0 || (*c >= 2 && *c < CAST_CHARACTERS && !o->cast) || (*c >= CAST_CHARACTERS && !enemy_cast_creatures(o))) return -1;
    for (j = 0; j < (int)(sizeof(kept) / sizeof(kept[0])); j++)
        if (kept[j].room == room && kept[j].rec == k) { *why = kept[j].why; return -1; }
    return *c < 2 ? look_model(*c) : *c == CAST_CROWD ? cast_crowd_model((unsigned)o->seed, scene, id, ver, o->cast_all, cast_off, o->cast_enemy, o->cast_enemy_off)
           : cast_model((unsigned)o->seed, scene, *c, o->cast_all, imports, cast_off, o->cast_enemy, o->cast_enemy_off);
}

/* enemyopt_glue.c, the end of the room load */
void cast_room(const EnemyOpts* o)
{
    int want[RECS], k, j, n = 0, room = room_file_now(), imports;
    char custom[40], enemies[48];
    if (getenv("RECVX_CAST_TRACE")) {
        const unsigned char* b = (const unsigned char*)sys->ev_flg;
        unsigned h = 2166136261u;
        for (k = 0; k < (int)((const unsigned char*)sys->ply_stflg - b); k++) h = (h ^ b[k]) * 16777619u;
        host_log("cast: room %04d loaded: state %08x, player %.2f %.2f %.2f hp %d\n", room, h, plp ? plp->px : 0.0f, plp ? plp->py : 0.0f, plp ? plp->pz : 0.0f, plp ? plp->hp : 0);
    }
    voice_end(NULL);
    pre.end = NULL;
    voice_room();
    if (o && o->voices && voices_want()) plan_want(o);
    if (!o || !rom || (!o->cast && !g_appear_per && !enemy_cast_creatures(o))) { cast_release(); return; }
    imports = import_count(o);
    for (k = 0; k < rom->ene_n && k < RECS; k++) {
        BH_PWORK* ep = &ene[k];
        const char* why = NULL;
        int c;
        want[k] = -1;
        if (!(ep->flg & 0x1) || !ep->mlwP) continue;
        want[k] = cast_want(o, room, k, ep->id, ep->mdlver, imports, &c, &why);
        if (why) host_log("cast: room %04d record %d (type %d) stays: %s\n", room, k, ep->id, why);
        if (c >= 0) rec_chr[k] = (unsigned char)c;
    }
    cast_release();
    for (k = 0; k < rom->ene_n && k < RECS; k++) {
        const ActorModel* am;
        const char* name;
        int s = -1;
        if (want[k] < 0) continue;
        am = want[k] >= ACTOR_PART ? NULL : actor_model(want[k]);
        name = am ? am->name : "a player model";
        for (j = 0; j < CSLOTS && s < 0; j++) if (slot_model[j] == want[k]) s = j;
        for (j = 0; j < CSLOTS && s < 0; j++)
            if (slot_model[j] < 0) {
                if (actor_set(ACTOR_CAST + j, want[k]) && actor_rigged(ACTOR_CAST + j)) { slot_model[j] = (short)want[k]; s = j; }
                else { actor_set(ACTOR_CAST + j, -1); break; }
            }
        if (s < 0) {
            host_log("cast: room %04d record %d (type %d model %d): %s %s, drawn as itself\n", room, k, ene[k].id, ene[k].mdlver, name,
                     j >= CSLOTS ? "has no display slot left" : "cannot be loaded or mapped");
            continue;
        }
        rec_slot[k] = (signed char)s;
        rec_id[k] = (unsigned char)ene[k].id;
        rec_ver[k] = ene[k].mdlver;
        actor_cast_hands(ACTOR_CAST + s);
        cast_n++;
        n++;
        host_log("cast: room %04d record %d (type %d model %d) -> %s (slot %d)\n", room, k, ene[k].id, ene[k].mdlver, name, s);
    }
    if (o->cast_all == 2) snprintf(custom, sizeof(custom), "custom models: %d checked", cast_custom_count(0, cast_off, imports, NULL));
    snprintf(enemies, sizeof(enemies), "enemy models: %s", o->cast_enemy == CAST_ENEMY_SAME ? "same as cast" : o->cast_enemy == CAST_ENEMY_ALL ? "all enemies" : o->cast_enemy == CAST_ENEMY_OFF ? "off" : "");
    if (o->cast_enemy == CAST_ENEMY_CUSTOM) snprintf(enemies, sizeof(enemies), "enemy models: %d checked", cast_custom_count(2, o->cast_enemy_off, 0, NULL));
    if (!o->cast) { host_log("cast: room %04d: %d records drawn as your look for Claire or Chris (per character) or as other monsters (%s, without the cast)\n", room, n, enemies); return; }
    host_log("cast: room %04d: %d records drawn as other models in cutscenes (seed %d, %s, %s%s, %s)\n", room, n, o->seed,
             o->cast_assign ? "random per cutscene" : "fixed", o->cast_all == 2 ? custom : o->cast_all ? "all models" : "cutscene models", imports ? ", imported models" : "", enemies);
}

static int role(int k) { return cast_role(rec_chr[k], sys->ply_id & 3, (sys->cb_flg & 0x80) ? sys->cng_pid & 3 : -1, -1); }
static int played(int k) { return role(k) == CAST_OWN; }

static int as_model(const BH_PWORK* ep, int k) { return k >= 0 && k < RECS && rec_slot[k] >= 0 && !played(k) && still(ep, k); }

void cast_show(void)
{
    while (hidden_n) hidden[--hidden_n]->evalflags &= ~0x8u;
}

extern int cast_hands;
static void hands_hide(BH_PWORK* ep, int who, int chr)
{
    int side, node, o, own, h = actor_cast_hands_chr(who);
    if (!cast_hands) return;
    for (side = 0; side < 2; side++) {
        if (!(own = actor_own_hand(who, side, &node)) && (h < 0 || h == chr)) continue;
        for (o = 4; o < 32; o++) {
            O_WRK* op = &sys->obwp[o];
            NJS_CNK_OBJECT* m = op->mlwP && op->mlwP->objP ? &op->mlwP->objP[0] : NULL;
            if (!(op->flg & 0x1) || !(op->flg & 0x80) || op->lkwkp != (unsigned char*)ep || op->lkono != node || !m) continue;
            if (op->mlwP->obj_num != 1) {
                if (getenv("RECVX_CAST_TRACE") && !(hid_logged[o >> 3] & (1 << (o & 7)))) {
                    hid_logged[o >> 3] |= (unsigned char)(1 << (o & 7));
                    host_log("cast: room %04d record %d (type %d): its hand piece (object %d, id %d, %d nodes) on node %d kept: a prop in it\n", room_file_now(),
                             (int)(ep - ene), ep->id, o, op->id, (int)op->mlwP->obj_num, node);
                }
                continue;
            }
            if ((m->evalflags & 0x8) || hidden_n == HIDDEN_MAX) continue;
            m->evalflags |= 0x8;
            hidden[hidden_n++] = m;
            if (getenv("RECVX_CAST_TRACE") && !(hid_logged[o >> 3] & (1 << (o & 7)))) {
                hid_logged[o >> 3] |= (unsigned char)(1 << (o & 7));
                host_log("cast: room %04d record %d (type %d): its hand piece (object %d, id %d) on node %d hidden: the model has %s\n", room_file_now(),
                         (int)(ep - ene), ep->id, o, op->id, node, own == 2 ? "no such hand" : own ? "a hand of its own" : h == 0 ? "Claire's hands" : h == 1 ? "Chris's hands"
                         : h == 2 ? "Steve's hands" : "Wesker's hands");
            }
        }
    }
}

extern int cast_parts;
static void parts_hide(BH_PWORK* ep, int who, int chr, int o)
{
    int i, m = actor_shown_model(who);
    if (!cast_parts || (m >= ACTOR_PART && ((m - ACTOR_PART) & 3) == chr)) return;
    for (; o < 32; o++) {
        O_WRK* op = &sys->obwp[o];
        if (!(op->flg & 0x1) || !(op->flg & 0x80) || op->lkwkp != (unsigned char*)ep || !actor_src_head(who, op->lkono) || !op->mlwP || !op->mlwP->objP) continue;
        for (i = 0; i < (int)op->mlwP->obj_num && hidden_n < HIDDEN_MAX; i++) {
            NJS_CNK_OBJECT* obj = &op->mlwP->objP[i];
            if (obj->evalflags & 0x8) continue;
            obj->evalflags |= 0x8;
            hidden[hidden_n++] = obj;
        }
        if (getenv("RECVX_CAST_TRACE") && !(hid_logged[o >> 3] & (1 << (o & 7)))) {
            hid_logged[o >> 3] |= (unsigned char)(1 << (o & 7));
            host_log("cast: room %04d record %d (type %d): its part (object %d, id %d, %d nodes) on head node %d hidden\n", room_file_now(), (int)(ep - ene), ep->id, o, op->id,
                     (int)op->mlwP->obj_num, op->lkono);
        }
    }
}

void cast_tick(void)
{
    static int was = -1;
    int k, now = cast_scene();
    if (now != was && getenv("RECVX_CAST_TRACE")) {
        host_log("cast: %s at game frame %d\n", now ? "a cutscene: the cast as their models" : "gameplay: the cast as themselves", (int)sys->gfrm_ct);
        for (k = 0; k < RECS; k++)
            if (rec_slot[k] >= 0) host_log("cast:  record %d (type %d) at %.1f %.1f %.1f%s\n", k, ene[k].id, ene[k].px, ene[k].py, ene[k].pz, (ene[k].stflg & 0x1000000) ? ", hidden" : "");
    }
    was = now;
    for (k = 0; k < (int)sys->ewk_n && k < RECS; k++) {
        BH_PWORK* ep = &ene[k];
        if (shown(rec_slot[k]) && (ep->flg & 0x1) && !still(ep, k) && !(lost_logged[k >> 3] & (1 << (k & 7)))) {
            lost_logged[k >> 3] |= (unsigned char)(1 << (k & 7));
            host_log("cast: room %04d record %d: given a model as type %d model %d, now type %d model %d: drawn as itself\n", room_file_now(), k,
                     rec_id[k], rec_ver[k], ep->id, ep->mdlver);
        }
        if (!as_model(ep, k) || (ep->stflg & 0x41000000) || !ep->mlwP) continue;
        actor_act(ACTOR_CAST + rec_slot[k], ep);
        hands_hide(ep, ACTOR_CAST + rec_slot[k], rec_chr[k]);
        parts_hide(ep, ACTOR_CAST + rec_slot[k], rec_chr[k], 2);
    }
}

void cast_double_parts(BH_PWORK* ep, int pid)
{
    parts_hide(ep, ACTOR_DOUBLE, pid, 3);
    hands_hide(ep, ACTOR_DOUBLE, pid);
}

static int creature_part(const BH_PWORK* ep)
{
    const BH_PWORK* l = (const BH_PWORK*)ep->lkwkp;
    int k;
    if (!(ep->flg & 0x80) || l < ene || l >= ene + RECS || (const unsigned char*)l != (const unsigned char*)&ene[l - ene]) return 0;
    k = (int)(l - ene);
    return rec_chr[k] >= CAST_CHARACTERS && as_model(l, k);
}

int cast_slot(const BH_PWORK* ep) { return ep >= ene && ep < ene + RECS && as_model(ep, (int)(ep - ene)) ? ACTOR_CAST + rec_slot[ep - ene] : -1; }

int cast_draw(BH_PWORK* ep)
{
    int k = (int)(ep - ene);
    if (creature_part(ep)) return 1;
    return as_model(ep, k) && actor_put_as(ACTOR_CAST + rec_slot[k], ep);
}

/* ---------------------------------------------------------------- "Randomize Voice Lines" (cast.h voices.c) */
int ADXF_LoadPartitionNw(int ptid, char* fname, void* dir, void* ptinfo);
int ADXF_GetPtStat(int ptid);
uint64_t SDL_GetTicks(void);
extern int PatId[4];
#define VOICE_PT 28
#define VOICE_FADE 7
extern unsigned int AdxPlayFlag[2];
extern ADXT_INFO AdxTInfo[4];
static struct {
    int on, rec, ms;
    const unsigned char* lip;
    uint64_t t0, cap;
    int fading;
    uint64_t iend, fend;
    int mouth, mroom;
    const void* mexp;
    const unsigned char* mlip;
    int gnext, gpt;
    int part;
    int snd;
    unsigned char hush[RECS];
} vl = { .gnext = -1 };

static int shown_slot(int k)
{
    if (cast_n && as_model(&ene[k], k)) return ACTOR_CAST + rec_slot[k];
    return actor_double_shown(&ene[k]) >= 0 ? ACTOR_DOUBLE : -1;
}

static int shown_model(int k)
{
    int who = shown_slot(k);
    return who < 0 ? -1 : actor_shown_model(who);
}

static const char* mouth_add(int sp, int vid, const unsigned char* lip, int sure)
{
    BH_PWORK* ep = &ene[sp];
    int who = shown_slot(sp);
    if (!g_voice_mouths) return "Talking Mouths is off";
    if (!lip) return "the inserted line has no lips";
    if (!sure && !voice_sure(room_file_now(), vid, sp)) return "the speaker is not sure";
    if (who < 0 || !(ep->flg & 0x1) || (ep->stflg & 0x41000000) || !ep->mlwP) return "its model is not drawn";
    if (!actor_talks(who)) return "its model has no mouth";
    if (!ep->mskp || !ep->exp0 || !ep->exp2) return "the record has no face";
    if (((int*)ep->exp0)[0] & 0x4) return "its mouth moves already";
    ((int*)ep->exp0)[268] = (int)(uintptr_t)lip;
    ((LIP_WORK*)ep->exp2)->flag = 0;
    ((int*)ep->exp0)[0] |= 0x4;
    vl.mouth = sp + 1; vl.mroom = room_file_now(); vl.mexp = ep->exp0; vl.mlip = lip;
    return NULL;
}

static void mouth_stop(void)
{
    BH_PWORK* ep = vl.mouth ? &ene[vl.mouth - 1] : NULL;
    vl.mouth = 0;
    if (!ep || vl.mroom != room_file_now() || ep->exp0 != vl.mexp || !ep->exp2 || ((LIP_WORK*)ep->exp2)->top != vl.mlip) return;
    ((int*)ep->exp0)[0] &= ~0x4;
    ((LIP_WORK*)ep->exp2)->flag = 0;
}

extern SND_REQ RequestInfo;
extern int pc_se_xbank;
#define CR_SLOT 10
static struct { int n, last, last_m, xb, k, seq_n, seq[3], len[3]; uint64_t end, cap; } cr;

static void cr_play(int prog)
{
    RequestInfo.SlotNo = CR_SLOT;
    RequestInfo.BankNo = 3;
    RequestInfo.ListNo = (char)prog;
    RequestInfo.Priority = 0;
    RequestInfo.Pan = 0; RequestInfo.PanDelayTime = 0;
    RequestInfo.Volume = 0; RequestInfo.VolumeDelayTime = 0;
    RequestInfo.Pitch = 0; RequestInfo.PitchDelayTime = 0;
    RequestInfo.FxInput = -1;
    pc_se_xbank = cr.xb;
    ExPlaySe(&RequestInfo);
    pc_se_xbank = 0;
}

uint64_t gs_frame_count(void);
static void mouth_trace(void)
{
    int k;
    for (k = 0; k < RECS && rom && k < rom->ene_n; k++) {
        const BH_PWORK* ep = &ene[k];
        int lips;
        if (!ep->mskp || !ep->exp0 || !ep->exp1 || !ep->exp2) continue;
        lips = (((const int*)ep->exp0)[0] & 0x4) != 0;
        if (!lips && !(vl.on && k == vl.rec) && atoi(getenv("RECVX_MOUTH_TRACE")) < 2) continue;
        host_log("mouth: frame %d rec %d lips %d jaw %.1f | voice %d fading %d creature %d line %d%s\n", (int)gs_frame_count(), k, lips,
                 ((const MASK_WORK*)ep->exp1)->param.jawang, AdxPlayFlag[1] != 0, vl.fading, cr.n && SDL_GetTicks() < cr.end,
                 vl.on && k == vl.rec ? vl.on : 0, vl.on && vl.hush[k] ? " hushed" : "");
    }
}

void pc_voice_tick(void)
{
    uint64_t now = SDL_GetTicks();
    if (vl.cap && now >= vl.cap) {
        if (AdxPlayFlag[1]) {
            RequestAdxFadeFunctionEx(1, -1, -127, VOICE_FADE);
            vl.fading = 1; vl.fend = now + VOICE_FADE_MS;
            host_log("voices: the inserted line is cut %d ms before its end (%d ms late), fading out over %d ms\n", (int)(vl.iend - now), (int)(now - vl.cap), VOICE_FADE_MS);
        }
        vl.cap = 0; vl.gnext = -1;
    }
    if (vl.gnext >= 0 && !AdxPlayFlag[1] && !vl.fading) {
        const VoiceCurated* c = voice_curated(vl.gpt - VOICE_PT + 1, vl.gnext);
        PlayVoiceEx2(vl.gpt, vl.gnext, NULL, 1, 0, 0);
        vl.gnext = c && c->next != 0xFFFF ? c->next : -1;
    }
    if (cr.n && cr.cap && now >= cr.cap) { StopSe(CR_SLOT); cr.n = 0; host_log("voices: the sound stops %d ms after the original's end\n", VOICE_GRACE_MS); }
    else if (cr.n && now >= cr.end + (cr.k + 1 < cr.seq_n ? VOICE_CR_GAP_MS : 0)) {
        if (++cr.k < cr.seq_n) { cr_play(cr.seq[cr.k]); cr.end = now + (uint64_t)cr.len[cr.k]; }
        else cr.n = 0;
    }
    if (vl.on) {
        int sounds = vl.snd == 1 ? AdxPlayFlag[1] && !vl.fading : vl.snd == 2 ? cr.n : 0, k;
        for (k = 0; k < RECS && rom && k < rom->ene_n; k++)
            if (vl.hush[k] && (k != vl.rec || !sounds) && ene[k].mskp && ene[k].exp0 && ene[k].exp2 && ((int*)ene[k].exp0)[0] & 0x4) {
                ((int*)ene[k].exp0)[0] &= ~0x4;
                ((LIP_WORK*)ene[k].exp2)->flag = 0;
            }
    }
    if (getenv("RECVX_MOUTH_TRACE")) mouth_trace();
}

static int creature_line(unsigned seed, int m, int disc, int vid, int sp, int own)
{
    unsigned char cand[64];
    int len[64], room = 0, n = 0, c, i, xb = 0, alt, ms, k, tot, at;
    char list[96];
    for (alt = 0; !n && (c = voice_creature(m, alt, &room, cand, 64)) > 0; alt++) {
        if (!(xb = snd_xbank_load(room))) continue;
        for (i = 0; i < c; i++)
            if ((len[n] = snd_xbank_prog_ms(xb, cand[i])) > 0) cand[n++] = cand[i];
    }
    if (!n) return 0;
    ms = adx_length_ms(PatId[1], vid);
    StopVoice(0);
    for (k = 0; k < n && cand[k] != cr.last; k++);
    cr.seq_n = voice_creature_plan(seed, room_file_now(), disc, vid, len, n, ms, VOICE_GRACE_MS, cr.last_m == m && k < n ? k : -1, cr.seq, 3);
    for (i = 0, tot = 0, at = 0; i < cr.seq_n; i++) {
        cr.len[i] = len[cr.seq[i]]; cr.seq[i] = cand[cr.seq[i]];
        tot += (i ? VOICE_CR_GAP_MS : 0) + cr.len[i];
        at += snprintf(list + at, sizeof list - (size_t)at, "%s%d (%d ms)", i ? ", " : "", cr.seq[i], cr.len[i]);
    }
    cr.last = cr.seq[cr.seq_n - 1]; cr.last_m = m;
    vl.on = 1; vl.rec = sp; vl.ms = ms; vl.lip = NULL; vl.t0 = SDL_GetTicks();
    vl.snd = 2; vl.hush[sp] = 1;
    if (ms > 0 && ms < VOICE_SHORT_MS && cr.len[0] > ms + VOICE_GRACE_MS) {
        host_log("voices: room %04d record %d: %s's line %d/%d (%d ms) is a short sound and %s has none: quiet\n", room_file_now(), sp, cast_name(own), disc, vid, ms, actor_model(m)->name);
        return 1;
    }
    host_log("voices: room %04d record %d: %s's line %d/%d (%d ms), shown as %s -> %d of its sounds from RM_%04d bank 3: %s%s\n", room_file_now(), sp,
             cast_name(own), disc, vid, ms, actor_model(m)->name, cr.seq_n, room, list, ms > 0 && tot > ms + VOICE_GRACE_MS ? ", none shorter: cut" : "");
    cr.xb = xb; cr.k = 0;
    cr_play(cr.seq[0]);
    cr.n = 1;
    cr.end = vl.t0 + (uint64_t)cr.len[0];
    cr.cap = ms > 0 && tot > ms + VOICE_GRACE_MS ? vl.t0 + (uint64_t)(ms + VOICE_GRACE_MS) : 0;
    return 1;
}

static int file_ms(int disc, int vid)
{
    static short len[2][2048];
    int d = disc - 1;
    if (d < 0 || d > 1 || vid < 0 || vid >= 2048) return 0;
    if (!len[d][vid]) len[d][vid] = (short)(adx_length_ms(VOICE_PT + d, vid) / 8 + 1);
    return (len[d][vid] - 1) * 8;
}

static int group_ms(int disc, int vid)
{
    const VoiceCurated* c;
    int ms = file_ms(disc, vid), k;
    for (k = 0; k < 8 && (c = voice_curated(disc, vid)) && c->next != 0xFFFF; k++) ms += file_ms(disc, vid = c->next);
    return ms;
}
static int line_ms(const VoiceLine* l) { return group_ms(l->disc, l->vid); }

static int part[2];
static void voice_parts(void)
{
    for (int d = 0; d < 2; d++)
        if (!part[d]) {
            ADXF_LoadPartitionNw(VOICE_PT + d, d ? (char*)"\\VOICE2.AFS" : (char*)"\\VOICE1.AFS", NULL, NULL);
            part[d] = ADXF_GetPtStat(VOICE_PT + d) == 3 ? 1 : -1;
        }
}

static const unsigned char* line_lips(const VoiceLine* l)
{
    static unsigned char* made[2][2048];
    static unsigned char lv[30 * 60];
    unsigned char out[sizeof lv * 2 + 1];
    const VoiceCurated* c = voice_curated(l->disc, l->vid);
    int d = l->disc - 1, n = 0, m, k, vid = l->vid;
    uint64_t t0 = SDL_GetTicks();
    if ((l->lip && !(c && c->next != 0xFFFF)) || d < 0 || d > 1 || l->vid >= 2048) return l->lip;
    if (made[d][l->vid]) return made[d][l->vid];
    for (k = 0; k < 8 && (m = adx_levels(VOICE_PT + d, vid, lv + n, (int)sizeof lv - n)) > 0; k++) {
        n += m;
        if (!(c = voice_curated(l->disc, vid)) || c->next == 0xFFFF) break;
        vid = c->next;
    }
    if (!(m = voice_lips_make(lv, n, out, (int)sizeof out)) || !(made[d][l->vid] = malloc((size_t)m))) return l->lip;
    memcpy(made[d][l->vid], out, (size_t)m);
    host_log("voices: line %d/%d's lips made from its sound (%d ms, %d bytes, %d ms to make)\n", l->disc, l->vid, n * 1000 / 30, m, (int)(SDL_GetTicks() - t0));
    return made[d][l->vid];
}

static unsigned short gpart = 0xFFFF;
static struct { int on; unsigned flag, play; } keep;
static int keep_playing(void)
{
    const unsigned char* p = bhScePtr;
    const EnemyOpts* o = enemy_opts();
    if (gpart == 0xFFFF || !p || !o || !o->voices || p[-8] != 25 || (p[-6] | p[-5] << 8) != gpart) return 0;
    keep.on = 1; keep.flag = AdxTInfo[1].Flag; keep.play = AdxPlayFlag[1];
    AdxTInfo[1].Flag = 0;
    return 1;
}

static int quiet(int sp, int ms)
{
    vl.on = 1; vl.rec = sp; vl.ms = ms; vl.lip = NULL; vl.t0 = SDL_GetTicks();
    vl.snd = 0; vl.hush[sp] = 1;
    return -1;
}

int pc_voice_on(int vid, int kind, int rec, const unsigned char* next, int* ptid)
{
    static int logged, unplanned;
    const EnemyOpts* o = enemy_opts();
    int disc = (sys->ss_flg & 0x1) + 1, sp = -1, own, chr, m, lrec, ms, lip, lms, before = 0, fit, sure = 0;
    VoiceChoice ch;
    const VoiceLine* l;
    const VoiceCurated *cv = voice_curated(disc, vid), *lc;
    const char* why;
    const unsigned char* lips;
    char from[12], cut[64], here[48] = "";
    unsigned short want = gpart;
    int gn = vl.gnext, full, pl = -1, gap = -1, ofs = rom && rom->evtp ? (int)(next - 8 - (const unsigned char*)rom->evtp) : -1;
#define KEEP(why) do { if (getenv("RECVX_CAST_TRACE") || cv) host_log("voices: room %04d line %d/%d kind %d rec %d, speaker %d (%s), shown as %d: stays (%s)\n", room_file_now(), disc, vid, kind, rec, sp, sp >= 0 && sp < RECS ? cast_name(cast_character(ene[sp].id, ene[sp].mdlver)) : "-", m, why); vl.cap = 0; return vid; } while (0)
    if (keep.on) { AdxTInfo[1].Flag = keep.flag; AdxPlayFlag[1] = keep.play; keep.on = 0; }
    if (ofs >= 0 && !getenv("RECVX_VOICE_WHOLE")) pl = voice_played_ms(room_file_now(), ofs, vid, &gap);
    if (getenv("RECVX_VOICE_TIME"))
        host_log("voicetime: %u on room %04d ofs %d line %d/%d (%d ms) played %d\n", (unsigned)(gs_frame_count() * 1000 / 30), room_file_now(),
                 rom && rom->evtp ? (int)(next - 8 - (const unsigned char*)rom->evtp) : -1, disc, vid, adx_length_ms(PatId[1], vid), pl);
    gpart = 0xFFFF;
    voice_end(want != 0xFFFF && vid == want ? NULL : "the next line");
    vl.gnext = -1;
    m = -1;
    if (!o || !o->voices || !rom) return vid;
    if ((lip = voice_lipset(next, 512, pc_insn_len_raw, &lrec)) >= 0) sp = lrec;
    else if (pre.task == bhCetask && pre.end && pre.end < next - 8 && next - 8 - pre.end <= 16 && voice_lipset_before(pre.end, next - 8, pc_insn_len_raw)) { sp = pre.rec; lip = 0; before = 1; }
    else if (kind == 1) sp = rec;
    else if (!kind) sp = voice_said_by(room_file_now(), vid);
    pre.end = NULL;
    pre.skip = lip >= 0 && !before ? bhCetask : NULL;
    if (cv && cv->flags & VOICE_CONFIRMED) {
        if (cv->chr < 0) KEEP("none of the cast says it, checked by hand");
        if (sp < 0 || sp >= rom->ene_n || sp >= RECS || cast_character(ene[sp].id, ene[sp].mdlver) != cv->chr) {
            for (sp = 0; sp < rom->ene_n && sp < RECS && cast_character(ene[sp].id, ene[sp].mdlver) != cv->chr; sp++);
            lip = -1; before = 0;
            if (sp >= rom->ene_n || sp >= RECS) { sp = -1; KEEP("its speaker, checked by hand, is not in the room"); }
        }
        sure = 1;
    }
    if (sp < 0 || sp >= rom->ene_n || sp >= RECS) KEEP("no speaker");
    own = cast_character(ene[sp].id, ene[sp].mdlver);
    m = shown_model(sp);
    chr = voice_model_chr(m);
    if (own >= 0 && chr < 0 && creature_line((unsigned)o->seed, m, disc, vid, sp, own)) return -1;
    if (own >= 0 && chr == own) KEEP("shown as its own character");
    if (own < 0 || chr < 0) KEEP(own < 0 ? "not a story character" : "shown as a model without lines");
    ms = adx_length_ms(PatId[1], vid);
    if (voice_count(0) < 0 || !plan_ready(o)) {
        voices_want();
        if (!logged++) host_log("voices: the lines %s: line %d/%d quiet\n", voice_count(0) < 0 ? "are not read yet" : "have no plan for these options yet", disc, vid);
        return quiet(sp, ms);
    }
    voice_parts();
    if (cv && cv->flags & VOICE_PART && vid == want) {
        gpart = cv->next; vl.gnext = gn;
        host_log("voices: room %04d record %d: line %d/%d (%d ms), a later part of a group said as one: quiet, the line in place of its first plays on\n", room_file_now(), sp, disc, vid, ms);
        quiet(sp, ms);
        vl.snd = vl.iend ? 1 : 0;
        vl.part = 1;
        return -1;
    }
    gpart = cv && cv->next != 0xFFFF && !(cv->flags & VOICE_PART) ? cv->next : 0xFFFF;
    full = gpart != 0xFFFF ? group_ms(disc, vid) : ms;
    if (own >= 2 && plan_find(room_file_now(), ofs, vid, chr, &l))
        ch = voice_choose_as(l, ms, full, pl, gap, cv && cv->flags & VOICE_GRUNT, line_ms);
    else {
        if (own >= 2 && unplanned++ < 32) host_log("voices: room %04d line %d/%d at %d, shown as %s: not in the plan\n", room_file_now(), disc, vid, ofs, cast_name(chr));
        ch = voice_choose((unsigned)o->seed, room_file_now(), disc, vid, chr, ms, full, pl, gap, cv && cv->flags & VOICE_GRUNT, line_ms, NULL);
    }
    if (ch.l && part[ch.l->disc - 1] < 0) { gpart = 0xFFFF; KEEP("the archive did not load"); }
    if (!ch.l) {
        host_log("voices: room %04d record %d: %s's line %d/%d (%d ms) shown as %s: quiet (%s)\n", room_file_now(), sp, cast_name(own), disc, vid, ms, cast_name(chr), ch.why);
        return quiet(sp, ms);
    }
    l = ch.l; fit = ch.fit; lms = ch.lms;
    lips = line_lips(l);
    why = lip < 0 ? mouth_add(sp, vid, lips, sure) : NULL;
    lc = voice_curated(l->disc, l->vid);
    snprintf(from, sizeof from, l->room != VOICE_NO_ROOM ? "RM_%04d" : "no scene", l->room);
    cut[0] = 0;
    if (pl > 0) snprintf(here, sizeof here, ", %d ms here, %d to the next", pl, gap);
    if (ch.cut == VOICE_CUT_FADE) snprintf(cut, sizeof cut, ", faded out by %d ms, when the next line starts", ch.stop);
    else if (ch.cut == VOICE_CUT_GRACE) snprintf(cut, sizeof cut, ", none out of the cooldown ends in time: cut at %d ms", fit);
    host_log("voices: room %04d record %d: %s's line %d/%d (%d ms%s%s%s), shown as %s -> %s's line %d/%d (%d ms%s%s%s) from %s, %s%s%s\n", room_file_now(), sp,
             cast_name(own), disc, vid, ms, here, sure ? ", speaker checked" : "", ch.grunts == VOICE_PICK_GRUNTS ? ", a grunt" : "",
             m >= ACTOR_PART ? "a player model" : actor_model(m) ? actor_model(m)->name : "?", cast_name(chr), l->disc, l->vid,
             lms, l->grunt ? ", a grunt" : "", lc && lc->next != 0xFFFF ? ", a group" : "",
             cut, from,
             lip >= 0 ? "with its lips" : sure ? "the speaker checked by hand: " : "no lips in the script (the speaker is the VoiceOn's record): ",
             lip >= 0 ? (!lips ? " (none: the script's)" : lips != l->lip ? " (made from its sound)" : "") : why ? "a still mouth, " : "Talking Mouths: its lips added", why ? why : "");
    vl.on = 1; vl.rec = sp; vl.ms = ms; vl.lip = lip >= 0 ? lips : NULL; vl.t0 = SDL_GetTicks();
    vl.snd = 1; vl.hush[sp] = 1;
    vl.gnext = lc && lc->next != 0xFFFF ? lc->next : -1; vl.gpt = VOICE_PT + l->disc - 1;
    if (before && lips && ene[sp].exp0 && ene[sp].exp2) {
        ((int*)ene[sp].exp0)[268] = (int)(uintptr_t)lips;
        ((LIP_WORK*)ene[sp].exp2)->flag = 0;
        vl.lip = NULL;
    }
    vl.iend = vl.t0 + (uint64_t)lms;
    vl.cap = !ch.cut ? 0 : vl.t0 + (uint64_t)(ch.cut == VOICE_CUT_FADE ? (ch.stop > VOICE_FADE_MS ? ch.stop - VOICE_FADE_MS : 0) : ch.stop);
    *ptid = VOICE_PT + l->disc - 1;
    return l->vid;
#undef KEEP
}

/* ---------------------------------------------------------------- Voice plan */
typedef struct {
    int room, m, chr, ms, pl, chose;
    const char *why, *kind, *res;
    VoiceChoice ch;
    VoiceOnAt v;
} PlanLine;
static struct { PlanLine* l; int n, cap; } plan;
static VoiceSaid plan_said;
static int plan_age(const VoiceLine* l) { return voice_said_age(&plan_said, l); }
static unsigned plan_rd32(const unsigned char* d, long n, long o) { return o >= 0 && o <= n - 4 ? d[o] | d[o + 1] << 8 | d[o + 2] << 16 | (unsigned)d[o + 3] << 24 : 0; }
static void plan_room(int room, const unsigned char* d, int n, void* ud)
{
    const EnemyOpts* o = ud;
    VoiceOnAt v[256];
    int c = voice_room_lines(room, d, n, pc_insn_len_raw, v, 256), i, k, j, ch, want[RECS], slot[RECS], sm[CSLOTS], ns = 0;
    const char* why[RECS];
    long rm = plan_rd32(d, n, 0x10), ep = plan_rd32(d, n, rm + 2 * 4);
    int nr = (int)plan_rd32(d, n, rm + 0x80 + 2 * 4);
    if (plan.n + c > plan.cap) {
        PlanLine* q = realloc(plan.l, sizeof(PlanLine) * (size_t)(plan.n + c + 1024));
        if (!q) return;
        plan.l = q; plan.cap = plan.n + c + 1024;
    }
    for (k = 0; k < RECS; k++) {
        long r = ep + k * 0x24L;
        why[k] = NULL; want[k] = slot[k] = -1;
        if (k >= nr || !ep || r + 0x24 > n) continue;
        want[k] = cast_want(o, room, k, d[r + 4] | d[r + 5] << 8, d[r + 9], import_count(o), &ch, &why[k]);
        if (want[k] < 0) continue;
        for (j = 0; j < ns && sm[j] != want[k]; j++);
        if (j == ns && ns < CSLOTS) sm[ns++] = want[k];
        if (j < ns) slot[k] = j; else why[k] = "no display slot left";
    }
    for (i = 0; i < c; i++) {
        PlanLine* p = &plan.l[plan.n++];
        int sp = v[i].sp;
        memset(p, 0, sizeof *p);
        p->room = room; p->v = v[i]; p->m = -1;
        if (v[i].own < 0 || sp < 0 || sp >= RECS) continue;
        if (v[i].own < 2) p->m = look_model(v[i].own);
        else if (slot[sp] >= 0) p->m = want[sp];
        else p->why = why[sp];
    }
}
static int plan_cmp(const void* a, const void* b)
{
    const PlanLine *x = a, *y = b;
    if (x->room != y->room) return x->room - y->room;
    if (x->v.script != y->v.script) return x->v.script - y->v.script;
    return x->v.ofs - y->v.ofs;
}

static int plan_make(const EnemyOpts* o)
{
    int i, k, gpart_ = 0xFFFF;
    voice_parts();
    plan.n = 0; plan_said.n = 0;
    if (room_each(plan_room, (void*)o) <= 0) return -1;
    qsort(plan.l, (size_t)plan.n, sizeof(PlanLine), plan_cmp);
    for (i = 0; i < plan.n; i++) {
        PlanLine* p = &plan.l[i];
        const VoiceOnAt* v = &p->v;
        const VoiceCurated* cv = voice_curated(v->disc, v->vid);
        int first = !i || p->room != plan.l[i - 1].room || v->script != plan.l[i - 1].v.script, gap, full;
        unsigned short want = first ? 0xFFFF : (unsigned short)gpart_;
        p->ms = adx_length_ms(VOICE_PT + v->disc - 1, v->vid); p->pl = voice_played_ms(p->room, v->ofs, v->vid, &gap); p->chr = -1;
        p->kind = "original";
        gpart_ = 0xFFFF;
        if (v->own == -2) p->res = "none of the cast says it, checked by hand";
        else if (v->own == -3) p->res = "its speaker, checked by hand, is not in the room";
        else if (v->sp < 0) p->res = "no speaker";
        else if (v->own < 0) p->res = "not a story character";
        else {
            int chr = p->chr = voice_model_chr(p->m);
            if (chr < 0 && p->m >= 0 && voice_creature(p->m, 0, &k, (unsigned char[64]){ 0 }, 64) > 0) { p->kind = "enemy"; p->res = "shown as an enemy: its sounds"; }
            else if (chr == v->own) {
                for (k = 0; v->own >= 2 && k < voice_count(chr); k++)
                    if (voice_line(chr, k)->disc == v->disc && voice_line(chr, k)->vid == v->vid) { voice_said_add(&plan_said, voice_line(chr, k)); break; }
                p->res = "shown as its own character";
            } else if (chr < 0) p->res = p->m >= 0 ? "shown as a model without lines" : p->why ? p->why : "drawn as itself";
            else if (cv && cv->flags & VOICE_PART && v->vid == want) { gpart_ = cv->next; p->kind = "group_silent"; p->res = "a later part of a group: the line in place of its first plays on"; }
            else {
                gpart_ = cv && cv->next != 0xFFFF && !(cv->flags & VOICE_PART) ? cv->next : 0xFFFF;
                full = gpart_ != 0xFFFF ? group_ms(v->disc, v->vid) : p->ms;
                p->ch = voice_choose((unsigned)o->seed, p->room, v->disc, v->vid, chr, p->ms, full, p->pl, gap, cv && cv->flags & VOICE_GRUNT, line_ms,
                                     v->own >= 2 ? plan_age : NULL);
                p->chose = 1;
                if (p->ch.l && v->own >= 2) voice_said_add(&plan_said, p->ch.l);
                if (p->ch.l) { p->kind = p->ch.l->grunt ? "grunt" : "line"; p->res = p->ch.grunts == VOICE_PICK_GRUNTS ? "a grunt for a grunt" : "picked"; }
                else { p->kind = "quiet"; p->res = p->ch.why; }
            }
        }
    }
    return plan.n;
}

typedef struct { int room, ofs, vid, chr; const VoiceLine* l; } PlanPick;
static PlanPick* picks;
static int picks_n;
static unsigned picks_key;
static int pick_cmp(const void* a, const void* b)
{
    const PlanPick *x = a, *y = b;
    return x->room != y->room ? x->room - y->room : x->ofs - y->ofs;
}
static int plan_find(int room, int ofs, int vid, int chr, const VoiceLine** l)
{
    PlanPick k = { room, ofs, 0, 0, NULL };
    const PlanPick* p = picks ? bsearch(&k, picks, (size_t)picks_n, sizeof k, pick_cmp) : NULL;
    if (!p || p->vid != vid || p->chr != chr) return 0;
    *l = p->l;
    return 1;
}

#define PLAN_FORMAT 2
static unsigned plan_key(const EnemyOpts* o)
{
    int imports = import_count(o);
    unsigned v[10] = { voices_cat_key(), voice_played_hash(), PLAN_FORMAT, (unsigned)o->seed, (unsigned)o->cast, (unsigned)o->cast_all,
                       (unsigned)o->cast_assign, (unsigned)o->cast_enemy, (unsigned)imports, imports ? cast_import_id() : 0u }, h = 2166136261u;
    const unsigned char* b;
    size_t i;
    if (!v[0]) return 0;
    for (b = (const unsigned char*)v, i = 0; i < sizeof v; i++) h = (h ^ b[i]) * 16777619u;
    for (b = o->cast_enemy_off, i = 0; i < sizeof o->cast_enemy_off; i++) h = (h ^ b[i]) * 16777619u;
    for (b = cast_off, i = 0; i < CAST_OFF_BYTES; i++) h = (h ^ b[i]) * 16777619u;
    return h ? h : 1;
}

static int plan_ready(const EnemyOpts* o) { return picks_key && picks_key == plan_key(o); }

#define PLAN_REC 14
static void plan_path(char* p, size_t n) { voices_cache_path(p, n, "voices.plan"); }
static int plan_fill(FILE* f, void* ud)
{
    unsigned char h[16] = "RVXP", r[PLAN_REC];
    unsigned key = *(const unsigned*)ud, n = (unsigned)picks_n;
    int i;
    for (i = 0; i < 4; i++) { h[4 + i] = (unsigned char)(1u >> 8 * i); h[8 + i] = (unsigned char)(key >> 8 * i); h[12 + i] = (unsigned char)(n >> 8 * i); }
    if (fwrite(h, 1, sizeof h, f) != sizeof h) return -1;
    for (i = 0; i < picks_n; i++) {
        const PlanPick* p = &picks[i];
        unsigned pv = p->l ? p->l->vid : 0;
        r[0] = (unsigned char)p->room; r[1] = (unsigned char)(p->room >> 8);
        r[2] = (unsigned char)p->ofs; r[3] = (unsigned char)(p->ofs >> 8); r[4] = (unsigned char)(p->ofs >> 16); r[5] = (unsigned char)(p->ofs >> 24);
        r[6] = (unsigned char)p->vid; r[7] = (unsigned char)(p->vid >> 8); r[8] = (unsigned char)p->chr; r[9] = p->l ? p->l->disc : 0;
        r[10] = (unsigned char)pv; r[11] = (unsigned char)(pv >> 8); r[12] = r[13] = 0;
        if (fwrite(r, 1, PLAN_REC, f) != PLAN_REC) return -1;
    }
    return 0;
}
static int plan_load(unsigned key)
{
    char path[1100];
    unsigned char h[16], r[PLAN_REC];
    unsigned n;
    int i, k, ok = 1;
    PlanPick* q;
    FILE* f;
    plan_path(path, sizeof path);
    if (!(f = pc_fopen(path, "rb"))) return 0;
    n = fread(h, 1, sizeof h, f) == sizeof h ? h[12] | h[13] << 8 | h[14] << 16 | (unsigned)h[15] << 24 : 0;
    if (!n || n > 65536 || memcmp(h, "RVXP\1\0\0\0", 8) || (unsigned)(h[8] | h[9] << 8 | h[10] << 16 | (unsigned)h[11] << 24) != key ||
        !(q = calloc(n, sizeof *q))) { fclose(f); return 0; }
    for (i = 0; ok && i < (int)n; i++) {
        PlanPick* p = &q[i];
        int disc, pv;
        if (fread(r, 1, PLAN_REC, f) != PLAN_REC) { ok = 0; break; }
        p->room = r[0] | r[1] << 8; p->ofs = (int)(r[2] | r[3] << 8 | r[4] << 16 | (unsigned)r[5] << 24); p->vid = r[6] | r[7] << 8; p->chr = r[8];
        disc = r[9]; pv = r[10] | r[11] << 8;
        for (k = 0; disc && k < voice_count(p->chr) && !p->l; k++)
            if (voice_line(p->chr, k)->disc == disc && voice_line(p->chr, k)->vid == pv) p->l = voice_line(p->chr, k);
        if ((disc && !p->l) || (i && pick_cmp(&q[i - 1], p) >= 0)) ok = 0;
    }
    ok = ok && fgetc(f) == EOF;
    fclose(f);
    if (!ok) { free(q); return 0; }
    free(picks); picks = q; picks_n = (int)n;
    return 1;
}

static void plan_want(const EnemyOpts* o)
{
    uint64_t t0 = SDL_GetTicks();
    unsigned key = plan_key(o);
    int i, n = 0;
    char path[1100];
    PlanPick* q;
    if (!key || key == picks_key) return;
    if (plan_load(key)) { picks_key = key; host_log("voices: the plan for seed %d from voices.plan, %d lines (%u ms)\n", o->seed, picks_n, (unsigned)(SDL_GetTicks() - t0)); return; }
    if (plan_make(o) < 0) { host_log("voices: cannot read the room files for the plan\n"); return; }
    for (i = 0; i < plan.n; i++) n += plan.l[i].chose && plan.l[i].v.own >= 2;
    if (!(q = calloc((size_t)n + 1, sizeof *q))) return;
    for (i = 0, n = 0; i < plan.n; i++)
        if (plan.l[i].chose && plan.l[i].v.own >= 2) q[n++] = (PlanPick){ plan.l[i].room, plan.l[i].v.ofs, plan.l[i].v.vid, plan.l[i].chr, plan.l[i].ch.l };
    qsort(q, (size_t)n, sizeof *q, pick_cmp);
    free(picks); picks = q; picks_n = n; picks_key = key;
    plan_path(path, sizeof path);
    if (pc_save_file(path, "wb", plan_fill, &key)) host_log("voices: cannot write %s\n", path);
    host_log("voices: the plan for seed %d made: %d lines, %d chosen (%u ms)\n", o->seed, plan.n, n, (unsigned)(SDL_GetTicks() - t0));
}

void pc_voice_lip(BH_PWORK* pp, int rec)
{
    if (pre.skip == bhCetask) pre.skip = NULL;
    else { pre.task = bhCetask; pre.end = bhScePtr; pre.rec = rec; }
    if (vl.mouth && rec == vl.mouth - 1 && !vl.part) mouth_stop();
    if (pp >= ene && pp < ene + RECS) vl.hush[pp - ene] = vl.on == 1;
    if (vl.on != 1 || !vl.lip || rec != vl.rec || !pp->exp0 || !pp->exp2) return;
    ((int*)pp->exp0)[268] = (int)(uintptr_t)vl.lip;
    if (getenv("RECVX_CAST_TRACE")) host_log("voices: record %d moves its mouth with the inserted line's lips (bank %d)\n", rec, ((int*)pp->exp0)[4]);
    vl.lip = NULL;
}

int pc_voice_playing(int flag)
{
    if (!vl.on || !vl.ms) return flag;
    if (vl.on == 1 && SDL_GetTicks() - vl.t0 < (uint64_t)vl.ms) return 1;
    vl.on = 2;
    return 0;
}

static void voice_end(const char* by)
{
    uint64_t now = SDL_GetTicks();
    if (by && vl.iend && now < vl.iend && AdxPlayFlag[1]) {
        if (!vl.fading) host_log("voices: the inserted line is cut hard by %s %d ms before its end\n", by, (int)(vl.iend - now));
        else if (now < vl.fend) host_log("voices: the inserted line's fade-out is cut by %s %d ms before its end\n", by, (int)(vl.fend - now));
    }
    if (by) vl.iend = 0;
    if (vl.fading || (vl.cap && now >= vl.cap)) {
        AdxTInfo[1].FadeFunc = 0;
        StopAdx(1);
        AdxPlayFlag[1] = 0;
        vl.cap = 0; vl.fading = 0;
    }
    vl.on = 0; vl.part = 0;
    memset(vl.hush, 0, sizeof vl.hush);
    if (vl.mouth && by) mouth_stop();
    if (cr.n) { cr.n = 0; StopSe(CR_SLOT); }
}

static void voice_room(void) { cr.last = -1; }

void pc_voice_stop(void)
{
    const unsigned char* p = bhScePtr;
    if (keep_playing()) return;
    if (getenv("RECVX_VOICE_TIME")) host_log("voicetime: %u stop\n", (unsigned)(gs_frame_count() * 1000 / 30));
    voice_end(vl.iend && p && p[-8] == 25 ? "the next line" : "StopVoice");
    vl.cap = 0;
}

int cast_describe(char* buf, int n)
{
    int k, len = 0, r;
    buf[0] = 0;
    for (k = 0; k < RECS && len < n - 1; k++) {
        if (!shown(rec_slot[k]) || !still(&ene[k], k)) continue;
        r = snprintf(buf + len, (size_t)(n - len), "%sr%d %s(%d)->%s%s", len ? ", " : "", k, cast_name(rec_chr[k]), rec_id[k],
                     cast_entry_name(slot_model[rec_slot[k]]),
                     played(k) ? (rec_chr[k] < 2 ? " (played: the double)" : " (played: as itself)") : rec_chr[k] < 2 ? " (your look)" : "");
        len = r < 0 || r >= n - len ? n - 1 : len + r;
    }
    return len;
}
