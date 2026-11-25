/* Object model loader and renderer for "Extra item boxes and herbs", game thread.
 *
 * Each catalogue model (pc/src/host/placer.c) is read once from its source room file
 * (actor_room_file) into a buffer inside the address window and kept for the session; its
 * textures stay loaded until njReleaseTextureAll (then everything is read again when next
 * needed):
 *  - an object or item record: its model chunk (MLB, an object's optionally behind its SKIN data),
 *    realized as room.c bhSetObjMdl / bhSetItmMdl do, with its texture list uploaded whole;
 *  - room model nodes (the item box's body, the typewriter): the nodes with their subtrees copied
 *    out of the room model (objects, models, vertex and polygon lists), each root put where the
 *    room has it, the whole moved so the origin is the bottom centre of its extent; of the room's
 *    texture list only the pictures it uses are uploaded (its polygon lists renumbered to them).
 * Extras in the current room are drawn after the room's own objects and items (game.c
 * bhAllDrawModel) with the items' lighting.
 */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/system.h>
#include <ps2/veronica/prog/binfunc.h>
#include <ps2/veronica/prog/njplus.h>
#include <ps2/veronica/prog/MdlPut.h>
#include "lp64_realize.h"
#include <ps2/veronica/prog/pwksub.h>
#include <ps2/veronica/prog/hitchk.h>
#include <ps2/veronica/prog/light.h>
#include <ps2/veronica/prog/ps2_texture.h>
#include <ps2/veronica/prog/ps2_NaTextureFunction.h>
#include <ps2/veronica/prog/ps2_NaMatrix.h>
#include <ps2/veronica/prog/ps2_NinjaCnk.h>
#include "../src/host/placer.h"
#include "../include/pc_parse.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);
const unsigned char* actor_room_file(int rm, int* len, unsigned char** scratch, int* cap);   /* actor_glue.c */
void actor_tex_free(int* entries, int* heap);

#define KEEP (160 << 10)                /* all models with their work (131 KB); inside the address window */
#define NODES 24
#define TEXN 16
#define TEX_MARGIN 32                   /* texture entries and heap left for the rooms to come (as actor_glue.c) */
#define HEAP_MARGIN (2 << 20)

typedef struct Part {
    O_WORK ow[NODES];                   /* (16-byte aligned: the part is carved at 64) */
    O_WRK w;
    NJS_TEXLIST tl;                     /* room model nodes: the room's pictures they use */
    NJS_TEXNAME tn[TEXN];
} Part;
static Part* part[PL_TYPES][2];         /* [1]: drawn with [0], linked to its node 0 (the item box's lid) */
static int lid_ax0[PL_TYPES];           /* the lid's own ax where the room has it (closed) */
static unsigned char keep[KEEP] __attribute__((aligned(64)));
static int keep_used;

static unsigned int rd32(const unsigned char* p) { unsigned int v; memcpy(&v, p, 4); return v; }
static unsigned short rd16(const unsigned char* p) { unsigned short v; memcpy(&v, p, 2); return v; }
static float rdf(const unsigned char* p) { float v; memcpy(&v, p, 4); return v; }

/* n bytes of keep at an address that is `mod` past a multiple of 64 (data keeps its alignment) */
static unsigned char* keep_take(int n, int mod)
{
    int at = ((keep_used + 63) & ~63) + mod;
    if (at + n > KEEP) return NULL;
    keep_used = at + n;
    return keep + at;
}

/* ---------------------------------------------------------------- the source room file */
static const unsigned char* rf;         /* expanded, rlen bytes (actor_glue.c's buffer) */
static int rlen;
static unsigned char* scratch;
static int scratch_cap;
static const char* why;                 /* the reason a load failed */

static int ok_range(unsigned int off, unsigned int len) { return pc_range_ok(off, len, (unsigned int)rlen); }

/* the data from `off` to the next offset any table of the file names (a model chunk's length) */
static int chunk_len(unsigned int off)
{
    const RDT_WORK* h = (const RDT_WORK*)rf;
    const unsigned char* rw = rf + h->hed00;   /* the disc's 32-bit ROM_WORK (LP64 layout differs) */
    unsigned int best = (unsigned int)rlen, i, n, v;
    unsigned int ene_n = lp64_rom_field(rw, LP64_RO_OFF_ENE_N);
    unsigned int obj_n = lp64_rom_field(rw, LP64_RO_OFF_OBJ_N);
    unsigned int itm_n = lp64_rom_field(rw, LP64_RO_OFF_ITM_N);
    unsigned int heads[4] = { h->hed01, h->hed02, h->hed03, h->hed04 };
    for (i = 0; i < 4; i++) if (heads[i] > off && heads[i] < best) best = heads[i];
    n = 1 + ene_n + (obj_n > 4 ? obj_n - 4 : 0) + itm_n;
    for (i = 0; i < n && ok_range(h->hed01 + 4 * i, 4); i++) if ((v = rd32(rf + h->hed01 + 4 * i)) > off && v < best) best = v;
    for (i = 0; i < ene_n + 3 && ok_range(h->hed02 + 4 * i, 4); i++)
        if ((v = rd32(rf + h->hed02 + 4 * i)) > off && v < best) best = v;
    if (ok_range(h->hed04, 4))
        for (i = 0, n = rd32(rf + h->hed04); i < n && ok_range(h->hed04 + 4 + 4 * i, 4); i++)
            if ((v = rd32(rf + h->hed04 + 4 + 4 * i)) > off && v < best) best = v;
    return (int)(best - off);
}

/* an object chunk's MLB (bhSetObjMdl: "MDL", or u32 skin size, u32, SKIN data, MLB) */
static const unsigned char* obj_mlb(const unsigned char* c) { return memcmp(c, "MDL", 3) ? c + rd32(c) + 8 : c; }
static int mlb_textured(const unsigned char* m) { return (int)rd32(m + 8) != -1; }

/* ---------------------------------------------------------------- textures */
static int tex_room(int texn)
{
    int entries, heap;
    actor_tex_free(&entries, &heap);
    if (entries >= texn + TEX_MARGIN && heap >= HEAP_MARGIN) return 1;
    why = "not enough texture memory left";
    return 0;
}

/* the TIM2 chunks of a texture list, one picture each (the lists of these sources have no PLI) */
static const unsigned char* tim_at(unsigned int list, int k)
{
    unsigned int p = list;
    for (;;) {
        if (!ok_range(p, 32) || memcmp(rf + p, "TIM2", 4)) return NULL;
        if (!k--) return ok_range(p + 32, rd32(rf + p + 4)) ? rf + p : NULL;
        p += 32 + rd32(rf + p + 4);
    }
}

/* ---------------------------------------------------------------- room model nodes */
#define RAW(i) (objs + (i) * 0x34)      /* NJS_CNK_OBJECT on the disc: offsets, -1 none */
static const unsigned char* objs;
static int nobj, obj_off;
static int raw_idx(int off) { return (off - obj_off) / 0x34; }
static int subtree(int i)
{
    int n = 1, c = (int)rd32(RAW(i) + 44);
    while (c != -1) { int k = raw_idx(c); n += subtree(k); c = (int)rd32(RAW(k) + 48); }
    return n;
}

/* chunk list lengths: vertex lists end with a 4-byte 255, polygon lists with a 2-byte one */
static int vlist_len(const unsigned char* p, const unsigned char* end)
{
    const unsigned char* q = p;
    while (q + 4 <= end) {
        if (q[0] == 255) return (int)(q + 4 - p);
        q += 4 + 4 * rd16(q + 2);
    }
    return -1;
}
static int plist_len(const unsigned char* p, const unsigned char* end, int* mapn, unsigned short* ids)
{
    const unsigned char* q = p;
    while (q + 2 <= end) {
        int t = q[0];
        if (t == 255) return (int)(q + 2 - p);
        if (t < 8) q += 2;
        else if (t < 16) {   /* texture id */
            int id = rd16(q + 2) & 0x1FFF, k;
            for (k = 0; k < *mapn && ids[k] != id; k++);
            if (k == *mapn) { if (k == TEXN) return -1; ids[(*mapn)++] = (unsigned short)id; }
            q += 4;
        } else q += 4 + 2 * rd16(q + 2);
    }
    return -1;
}
static void plist_renumber(unsigned char* p, const unsigned short* ids, int idn)
{
    for (;;) {
        int t = p[0];
        if (t == 255) return;
        if (t < 8) p += 2;
        else if (t < 16) {
            unsigned short v = rd16(p + 2);
            int k;
            for (k = 0; k < idn && ids[k] != (v & 0x1FFF); k++);
            v = (unsigned short)((v & ~0x1FFF) | k);
            memcpy(p + 2, &v, 2);
            p += 4;
        } else p += 4 + 2 * rd16(p + 2);
    }
}

/* the nodes d->rec[] of the room model (mlb), with their subtrees, as part p's model; *org: the first
   root's place in the room */
static int build_piece(const PlacerDef* d, const unsigned char* mlb, unsigned int texlist, Part* p, float* org)
{
    static short par[1024];
    short list[NODES];
    unsigned short ids[TEXN];
    int n = 0, idn = 0, r, i, j, roots = 0;
    const unsigned char* base = mlb + rd16(mlb + 4);
    const unsigned char* end = rf + rlen;
    NJS_CNK_OBJECT* out;
    unsigned char* tex;

    nobj = rd16(mlb + 6);
    obj_off = (int)rd32(mlb + 12);
    objs = base + obj_off;
    if (obj_off < 0 || nobj > 1024 || objs + nobj * 0x34 > end) { why = "bad room model"; return 0; }
    for (i = 0; i < nobj; i++) par[i] = -1;
    for (i = 0; i < nobj; i++)
        for (j = (int)rd32(RAW(i) + 44); j != -1; j = (int)rd32(RAW(raw_idx(j)) + 48)) par[raw_idx(j)] = (short)i;
    for (r = 0; r < 8 && d->rec[r] >= 0; r++) {
        int k = d->rec[r], s = k < nobj ? subtree(k) : NODES + 1;
        if (n + s > NODES) { why = "too many nodes"; return 0; }
        for (j = 0; j < s; j++) list[n++] = (short)(k + j);
        roots++;
    }
    out = (NJS_CNK_OBJECT*)keep_take(n * (int)sizeof(NJS_CNK_OBJECT), 0);
    if (!out) { why = "out of model memory"; return 0; }
    for (i = 0, r = 0; i < n; i++) {
        const unsigned char* o = RAW(list[i]);
        int moff = (int)rd32(o + 4), c = (int)rd32(o + 44), sb = (int)rd32(o + 48), root = 0;
        NJS_CNK_OBJECT* q = &out[i];
        for (j = 0; j < roots; j++) root |= list[i] == d->rec[j];
        q->evalflags = rd32(o);
        for (j = 0; j < 3; j++) { q->pos[j] = rdf(o + 8 + 4 * j); q->ang[j] = (int)rd32(o + 20 + 4 * j); q->scl[j] = rdf(o + 32 + 4 * j); }
        q->child = NULL;
        q->sibling = NULL;
        for (j = 0; j < n; j++) {
            if (c != -1 && list[j] == raw_idx(c)) q->child = &out[j];
            if (!root && sb != -1 && list[j] == raw_idx(sb)) q->sibling = &out[j];
        }
        if (root) {   /* where the room has it: its ancestors' places added (no ancestor may turn) */
            int a;
            for (a = par[list[i]]; a >= 0; a = par[a]) {
                if (rd32(RAW(a) + 20) || rd32(RAW(a) + 24) || rd32(RAW(a) + 28)) { why = "a turned parent node"; return 0; }
                for (j = 0; j < 3; j++) q->pos[j] += rdf(RAW(a) + 8 + 4 * j);
            }
            if (!r++) memcpy(org, q->pos, sizeof(q->pos));
            for (j = i + 1; j < n; j++)
                for (a = 0; a < roots; a++) if (list[j] == d->rec[a] && !q->sibling) q->sibling = &out[j];
        }
        q->model = NULL;
        if (moff != -1) {
            const unsigned char* m = base + moff;
            NJS_CNK_MODEL* nm;
            int vo, po, vl, pl;
            if (m + 24 > end) { why = "bad node"; return 0; }
            vo = (int)rd32(m);
            po = (int)rd32(m + 4);
            vl = vo == -1 ? 0 : vo < 0 ? -1 : vlist_len(base + vo, end);
            pl = po == -1 ? 0 : po < 0 ? -1 : plist_len(base + po, end, &idn, ids);
            if (vl < 0 || pl < 0) { why = "bad node data (or more than 16 pictures)"; return 0; }
            nm = (NJS_CNK_MODEL*)keep_take(24, 0);
            if (!nm) { why = "out of model memory"; return 0; }
            nm->vlist = NULL;
            nm->plist = NULL;
            nm->center.x = rdf(m + 8); nm->center.y = rdf(m + 12); nm->center.z = rdf(m + 16);
            nm->r = fabsf(rdf(m + 20));
            if (vl && (nm->vlist = (Sint32*)keep_take(vl, (int)((uintptr_t)(base + vo) & 63)))) memcpy(nm->vlist, base + vo, vl);
            if (pl && (nm->plist = (Sint16*)keep_take(pl, (int)((uintptr_t)(base + po) & 63)))) memcpy(nm->plist, base + po, pl);
            if ((vl && !nm->vlist) || (pl && !nm->plist)) { why = "out of model memory"; return 0; }
            if (pl) plist_renumber((unsigned char*)nm->plist, ids, idn);
            q->model = nm;
        }
    }
    /* the pictures it uses, in their new order, as a texture list of their own */
    for (i = 0, j = 0; i < idn; i++) {
        const unsigned char* t = tim_at(texlist, ids[i]);
        if (!t) { why = "a picture is not in the room's texture list"; return 0; }
        j += 32 + (int)rd32(t + 4);
    }
    if (j + 32 > scratch_cap) { why = "pictures too large"; return 0; }
    for (i = 0, tex = scratch; i < idn; i++) {
        const unsigned char* t = tim_at(texlist, ids[i]);
        memcpy(tex, t, 32 + rd32(t + 4));
        tex += 32 + rd32(t + 4);
    }
    memset(tex, 0, 32);
    memset(tex, 0xFF, 4);   /* end */
    p->w.mdl[0].flg = mlb[3];
    p->w.mdl[0].obj_num = (unsigned short)n;
    p->w.mdl[0].objP = out;
    npCnkFlatOff(out);   /* as bhSetRoom does for the room model */
    memset(p->tn, 0, sizeof(p->tn));
    p->tl.textures = p->tn;
    p->tl.nbTexture = (unsigned int)idn;
    p->w.mdl[0].texP = idn ? &p->tl : NULL;
    if (idn && !tex_room(idn)) return 0;
    if (idn) { bhSetMemPvpTexture(&p->tl, scratch, 0); p->w.mdl[0].flg |= 0x200; }
    return 1;
}

/* an object's or item's model chunk as part p's model, with its texture list (at file offset `list`, 0 none) */
static int build_record(unsigned int off, int is_obj, unsigned int list, Part* p)
{
    int len = chunk_len(off);
    unsigned char* c;
    const unsigned char* m;
    ML_WORK* ml = &p->w.mdl[0];
    if (len <= 16 || !ok_range(off, len)) { why = "bad model chunk"; return 0; }
    if (!(c = keep_take(len, (int)((uintptr_t)(rf + off) & 63)))) { why = "out of model memory"; return 0; }
    memcpy(c, rf + off, len);
    m = is_obj ? obj_mlb(c) : c;
    if (m < c || m + 16 > c + len) { why = "bad model chunk"; return 0; }
    bhMlbBinRealize((void*)m, ml);
    ml->owP = p->ow;   /* (the realizer clears it) */
    if (m != c) { p->w.skp[0] = (int*)(c + 8); npSkinConvert(ml->objP, p->w.skp[0]); }
    if (!ml->objP || ml->obj_num > NODES) { why = "too many nodes"; return 0; }
    if (ml->texP) {
        if (!list || !ok_range(list, 32)) { why = "no texture list"; return 0; }
        if (!tex_room((int)ml->texP->nbTexture)) return 0;
        bhSetMemPvpTexture(ml->texP, (unsigned char*)rf + list, 0);
        ml->flg |= 0x200;
    }
    return 1;
}

static void part_init(Part* p)
{
    memset(p, 0, sizeof(*p));
    p->w.mtx = (void*)p->w.mtxbuf;
    p->w.mlwP = &p->w.mdl[0];
    p->w.mdl[0].owP = p->ow;
    p->w.sx = p->w.sy = p->w.sz = p->w.sxb = p->w.syb = p->w.szb = 1.0f;
    p->w.clp_jno[1] = -1;
}

/* the extent of type t's parts at the origin, unturned (model space after bhCalcModel) */
static void extent(int t, float* lo, float* hi)
{
    int k, i, j, v;
    for (j = 0; j < 3; j++) { lo[j] = 1e9f; hi[j] = -1e9f; }
    for (k = 0; k < 2 && part[t][k]; k++) {
        ML_WORK* ml = &part[t][k]->w.mdl[0];
        if (!k) { part[t][0]->w.px = part[t][0]->w.py = part[t][0]->w.pz = 0; part[t][0]->w.ax = part[t][0]->w.ay = part[t][0]->w.az = 0; }
        bhCalcModel((BH_PWORK*)&part[t][k]->w);
        for (i = 0; i < (int)ml->obj_num; i++) {
            const NJS_CNK_MODEL* m = ml->objP[i].model;
            const unsigned char* q;
            if (!m || !m->vlist) continue;
            for (q = (const unsigned char*)m->vlist; q[0] != 255; q += 4 + 4 * rd16(q + 2)) {
                int first = q[0] == 51 ? 64 : q[0] == 41 ? 8 : 0, stride = q[0] == 51 ? 32 : 24, n = rd16(q + 6);
                if (!first) continue;
                for (v = 0; v < n; v++) {
                    NJS_POINT3 a, b;
                    memcpy(&a, q + first + stride * v, sizeof(a));
                    njCalcPoint((NJS_MATRIX*)&ml->owP[i].mtx, &a, &b);
                    for (j = 0; j < 3; j++) {
                        float c = j == 0 ? b.x : j == 1 ? b.y : b.z;
                        if (c < lo[j]) lo[j] = c;
                        if (c > hi[j]) hi[j] = c;
                    }
                }
            }
        }
    }
    if (lo[0] > hi[0]) for (j = 0; j < 3; j++) lo[j] = hi[j] = 0;
}

static int load_type(int t)
{
    const PlacerDef* d = &placer_defs[t];
    const RDT_WORK* h;
    const unsigned char* rw;
    const ETTY_WORK *ene, *obj, *itm;
    unsigned int mdltab, texn, ent[64];
    unsigned int ene_n, obj_n, itm_n, enep, objp, itmp;
#define LIST(k) (ent[k] < texn ? rd32(rf + h->hed04 + 4 + 4 * ent[k]) : 0u)
    int i, j, ne = 0, nobjs, e = 0, keep0 = keep_used, idx;
    float org[3] = { 0, 0, 0 };
    char name[16];

    why = "cannot read the room file";
    snprintf(name, sizeof(name), "RM_%04d", d->room);
    if (!(rf = actor_room_file(d->room, &rlen, &scratch, &scratch_cap))) goto fail;
    why = "bad room file";
    h = (const RDT_WORK*)rf;
    if (!ok_range(h->hed00, LP64_ROM_DISC_SIZE)) goto fail;
    rw = rf + h->hed00;   /* the disc's 32-bit ROM_WORK (LP64 layout differs) */
    ene_n = lp64_rom_field(rw, LP64_RO_OFF_ENE_N);
    obj_n = lp64_rom_field(rw, LP64_RO_OFF_OBJ_N);
    itm_n = lp64_rom_field(rw, LP64_RO_OFF_ITM_N);
    enep = lp64_rom_field(rw, LP64_RO_OFF_ENEP);
    objp = lp64_rom_field(rw, LP64_RO_OFF_OBJP);
    itmp = lp64_rom_field(rw, LP64_RO_OFF_ITMP);
    if (!ok_range(enep, ene_n * sizeof(ETTY_WORK)) ||
        !ok_range(objp, obj_n * sizeof(ETTY_WORK)) ||
        !ok_range(itmp, itm_n * sizeof(ETTY_WORK)) || obj_n > 64 || !ok_range(h->hed04, 4))
        goto fail;
    ene = (const ETTY_WORK*)(rf + enep);
    obj = (const ETTY_WORK*)(rf + objp);
    itm = (const ETTY_WORK*)(rf + itmp);
    for (i = 0; i < (int)ene_n; i++) ne += ene[i].flg & 0x1;
    if (ne) { why = "the source room has enemies"; goto fail; }
    nobjs = obj_n > 4 ? obj_n - 4 : 0;
    mdltab = h->hed01;
    texn = rd32(rf + h->hed04);
    if (!ok_range(mdltab, 4 * (1 + nobjs + itm_n)) || !ok_range(h->hed04 + 4, 4 * texn)) goto fail;
    for (i = 0; i < 1 + (int)nobjs + (int)itm_n; i++) if (!ok_range(rd32(rf + mdltab + 4 * i), 16)) goto fail;
    for (i = 0; i < 64; i++) ent[i] = (unsigned int)-1;
    e = mlb_textured(rf + rd32(rf + mdltab));
    for (i = 4; i < (int)obj_n; i++)
        if (mlb_textured(obj_mlb(rf + rd32(rf + mdltab + 4 * (1 + i - 4))))) {
            for (j = 4; j < i && !(obj[j].id == obj[i].id && ent[j] != (unsigned int)-1); j++);
            ent[i] = j < i ? ent[j] : (unsigned int)e++;
        }
    for (i = 0; i < (int)itm_n && i < 32; i++)
        if (mlb_textured(rf + rd32(rf + mdltab + 4 * (1 + nobjs + i)))) {
            for (j = 0; j < i && !(itm[j].id == itm[i].id && ent[32 + j] != (unsigned int)-1); j++);
            ent[32 + i] = j < i ? ent[32 + j] : (unsigned int)e++;
        }

    for (i = 0; i < (d->lid >= 0 ? 2 : 1); i++) {
        if (!(part[t][i] = (Part*)keep_take(sizeof(Part), 0))) { why = "out of model memory"; goto fail; }
        part_init(part[t][i]);
    }
    why = "bad record";
    idx = d->rec[0];
    if (d->kind == PLK_PIECE) {
        if (!mlb_textured(rf + rd32(rf + mdltab)) || !build_piece(d, rf + rd32(rf + mdltab), rd32(rf + h->hed04 + 4), part[t][0], org)) goto fail;
    } else if (d->kind == PLK_OBJ) {
        if (idx >= 4 && idx < (int)obj_n && d->item && obj[idx].id != d->item) { why = "the object record is another object"; goto fail; }
        if (idx < 4 || idx >= (int)obj_n || !build_record(rd32(rf + mdltab + 4 * (1 + idx - 4)), 1, LIST(idx), part[t][0])) goto fail;
    } else {
        if (idx < 0 || idx >= (int)itm_n || idx >= 32 || itm[idx].id != d->item) { why = "the item record is another item"; goto fail; }
        if (!build_record(rd32(rf + mdltab + 4 * (1 + nobjs + idx)), 0, LIST(32 + idx), part[t][0])) goto fail;
    }
    if (d->lid >= 0) {   /* linked to node 0, where the room has it next to the body */
        O_WRK* w = &part[t][1]->w;
        if (d->lid < 4 || d->lid >= (int)obj_n || !build_record(rd32(rf + mdltab + 4 * (1 + d->lid - 4)), 1, LIST(d->lid), part[t][1])) goto fail;
        w->flg = 0x80;
        w->lkwkp = (unsigned char*)&part[t][0]->w;
        w->lkono = 0;
        w->lox = obj[d->lid].px - org[0];
        w->loy = obj[d->lid].py - org[1];
        w->loz = obj[d->lid].pz - org[2];
        w->ax = lid_ax0[t] = obj[d->lid].ax; w->ay = obj[d->lid].ay; w->az = obj[d->lid].az;
    }
    extent(t, g_placer.type_min[t], g_placer.type_max[t]);
    if (d->kind == PLK_PIECE) {   /* the origin: the bottom centre of its extent */
        float c[3] = { (g_placer.type_min[t][0] + g_placer.type_max[t][0]) * 0.5f, g_placer.type_min[t][1],
                       (g_placer.type_min[t][2] + g_placer.type_max[t][2]) * 0.5f };
        NJS_CNK_OBJECT* o;
        for (o = part[t][0]->w.mdl[0].objP; o; o = o->sibling)
            for (j = 0; j < 3; j++) o->pos[j] -= c[j];
        for (j = 0; j < 3; j++) { g_placer.type_min[t][j] -= c[j]; g_placer.type_max[t][j] -= c[j]; }
    }
    g_placer.type_state[t] = 1;
    host_log("placer: %s from %s, extent x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f (%d KB of %d used)\n", d->name, name,
             g_placer.type_min[t][0], g_placer.type_max[t][0], g_placer.type_min[t][1], g_placer.type_max[t][1],
             g_placer.type_min[t][2], g_placer.type_max[t][2], keep_used >> 10, KEEP >> 10);
    return 1;
fail:
    part[t][0] = part[t][1] = NULL;
    keep_used = keep0;
    g_placer.type_state[t] = -1;
    host_log("placer: %s from %s: %s\n", d->name, name, why);
    g_placer.result = why;
    return 0;
}

void placer_textures_released(void)
{
    memset(part, 0, sizeof(part));
    keep_used = 0;
    memset(g_placer.type_state, 0, sizeof(g_placer.type_state));
}

static int room_ready(void)
{
    return sys && plp && rom && sys->rdtp && (sys->tk_flg & 0x80) && (sys->sp_flg & 0x2) && !(sys->cb_flg & 0x1);
}

void placer_frame(void)
{
    if (sys && plp && room_ready()) extras_load();
}

int placer_ready(int t)
{
    if (t < 0 || t >= placer_types()) return 0;
    if (!g_placer.type_state[t]) load_type(t);
    return g_placer.type_state[t] == 1;
}

/* type t drawn at pos, turned by ang, with the items' lighting (an object, the planter: the objects' ambient;
   objitm.c bhDrawObjItm); lid_ax turns its lid (the item box opening) */
void placer_put(int t, const float* pos, const int* ang, int lid_ax)
{
    int k, amb;
    if (t < 0 || t >= placer_types() || g_placer.type_state[t] != 1) return;
    amb = (unsigned char)(placer_defs[t].kind == PLK_OBJ ? rom->amb_obj : rom->amb_itm);
    njCnkSetEasyMultiAmbient(rom->amb_r[amb], rom->amb_g[amb], rom->amb_b[amb]);
    njCnkSetSimpleMultiAmbient(rom->amb_r[amb], rom->amb_g[amb], rom->amb_b[amb]);
    njSetMatrix(NULL, cam.mtx);
    if (!(sys->st_flg & 0x10000)) {
        njCnkSetEasyMultiLightColor(1, 0, 0, 0);
        njCnkSetEasyMultiLightSwitch(1, 0);
        njCnkSetSimpleMultiLightColor(1, 0, 0, 0);
        njCnkSetSimpleMultiLightSwitch(1, 0);
    } else {
        njCnkSetEasyMultiLightColor(1, sys->lg_r, sys->lg_g, sys->lg_b);
        njCnkSetEasyMultiLightVector(sys->lg_vx, sys->lg_vy, sys->lg_vz);
        njCnkSetEasyMultiLightSwitch(1, 1);
        njCnkSetEasyMultiLightMatrices();
        njCnkSetSimpleMultiLightColor(1, sys->lg_r, sys->lg_g, sys->lg_b);
        njCnkSetSimpleMultiLightVector(sys->lg_vx, sys->lg_vy, sys->lg_vz);
        njCnkSetSimpleMultiLightSwitch(1, 1);
        njCnkSetSimpleMultiLightMatrices();
    }
    for (k = 0; k < 2 && part[t][k]; k++) {
        O_WRK* w = &part[t][k]->w;
        if (!k) {
            w->px = pos[0]; w->py = pos[1]; w->pz = pos[2];
            w->ax = ang[0]; w->ay = ang[1]; w->az = ang[2];
        } else w->ax = (lid_ax0[t] + lid_ax) & 0xFFFF;
        bhCalcModel((BH_PWORK*)w);
        bhPutModel((BH_PWORK*)w);
    }
}

void placer_draw(void)
{
    if (!rom || !sys || !plp || !cam.mtx || !extras_here()) return;
    if (!(sys->st_flg & 0x20)) bhSetLight();
    njControl3D(0x100);
    extras_draw();
    njControl3D(0x100);
}
