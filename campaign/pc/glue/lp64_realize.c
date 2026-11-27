/* LP64 on-disc binary struct realization: see lp64_realize.h.
 *
 * Every unpack below is a field-exact transcription of the decomp's in-place
 * realize loops (room.c bhSetRoom, binfunc.c bhMlbBinRealize / bhBscBinRealize
 * / bhCnkBinRealize / bhMnbBinRealize) against the 32-bit disc layout, writing
 * host-layout copies into the arena. The disc bytes are never written:
 * the decomp's in-place sentinel clearing (nbPoint & 0x80000000, r < 0) reads
 * the sentinel from disc and clears it only in the arena copy.
 *
 * Faithfulness notes (32-bit behavior kept bit-for-bit):
 *  - ROM_WORK's first 16 pointer words are relocated UNCONDITIONALLY with the
 *    rdtp base, exactly like the decomp's word loop; dmp00..dmp15 keep their
 *    raw disc values (the decomp loop stops before dmp00); mdl is zeroed by
 *    bhSetRoom right after, so its disc bytes are dropped here too.
 *  - Fields whose decomp fixup has no NULL branch (NJS_MODEL.points,
 *    NJS_MESHSET.meshes, NJS_TEXNAME.filename) keep the raw 32-bit value
 *    (0xFFFFFFFF or the relocated address) rather than becoming NULL.
 *  - LGT_WORK's lkwkp/exp/light and CAM_KEYF_WORK/EVC's recp are runtime
 *    scratch (recp is never read anywhere in the decomp); the unpack zeroes
 *    them so the game's NULL writes land in 8-byte slots.
 */
#include "lp64_realize.h"

#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/binfunc.h>

#if UINTPTR_MAX > 0xFFFFFFFFu
#define ASHFORD_LP64 1
#else
#define ASHFORD_LP64 0
#endif

/* ------------------------------------------------------------------ arenas */

#if ASHFORD_LP64

#define LP64_ROOM_ARENA    (10u << 20)   /* room models, CUT/LGT/EVC arrays, EVT view */
#define LP64_SESSION_ARENA (16u << 20)   /* player, weapons, item-view models, motions, swapped (appearance/cast) models */

static unsigned char lp64_room_arena[LP64_ROOM_ARENA] __attribute__((aligned(64)));
static unsigned char lp64_session_arena[LP64_SESSION_ARENA] __attribute__((aligned(64)));

static unsigned char* lp64_bump[LP64_SLOT_COUNT];
static unsigned char* lp64_base[LP64_SLOT_COUNT];
static const unsigned char* lp64_limit[LP64_SLOT_COUNT];

/* Deduplication of unpacked Ninja binaries by disc base (rooms realize the
   same model set once per enemy/object/item that shares it). The disc base alone is not an identity:
   equipped weapons and items are copied into the same fixed buffers with different contents, so an
   entry also carries a signature of the structure it unpacked (header, object/motion table, model
   headers, texture table) and only a hit on both is reused. */
#define LP64_DEDUP_MAX 2048   /* RM_0020 alone needs well over 128 distinct binaries; 2 slots x 2048 x 40 bytes */
typedef struct {
    const void* bin;
    uint32_t sig;  /* lp64_ml_sig / lp64_mn_sig of the content unpacked */
    void* objP;    /* unpacked NJS_CNK_OBJECT array (ML) */
    void* texP;    /* unpacked NJS_TEXLIST (ML) */
    void* md2P;    /* unpacked NJS_MDATA2_MOD array (MN) */
} LP64_DEDUP;
static LP64_DEDUP lp64_dedup[LP64_SLOT_COUNT][LP64_DEDUP_MAX];
static int lp64_dedup_n[LP64_SLOT_COUNT];

static void lp64_arena_init(void)
{
    if (lp64_bump[LP64_SLOT_ROOM]) return;
    lp64_base[LP64_SLOT_ROOM] = lp64_bump[LP64_SLOT_ROOM] = lp64_room_arena;
    lp64_limit[LP64_SLOT_ROOM] = lp64_room_arena + sizeof(lp64_room_arena);
    lp64_base[LP64_SLOT_SESSION] = lp64_bump[LP64_SLOT_SESSION] = lp64_session_arena;
    lp64_limit[LP64_SLOT_SESSION] = lp64_session_arena + sizeof(lp64_session_arena);
}

static unsigned char* lp64_alloc(int slot, size_t size, size_t align)
{
    unsigned char* p;
    uintptr_t mask;
    lp64_arena_init();
    if ((unsigned)slot >= LP64_SLOT_COUNT) host_fatal("lp64: bad slot %d\n", slot);
    p = lp64_bump[slot];
    mask = align - 1;
    p = (unsigned char*)(((uintptr_t)p + mask) & ~(uintptr_t)mask);
    if ((size_t)(lp64_limit[slot] - p) < size)
        host_fatal("lp64 arena exhausted (slot %d, need %zu bytes)\n", slot, size);
    lp64_bump[slot] = p + size;
    return p;
}

static const LP64_DEDUP* lp64_dedup_find(int slot, const void* bin, uint32_t sig)
{
    int i;
    for (i = 0; i < lp64_dedup_n[slot]; i++)
        if (lp64_dedup[slot][i].bin == bin && lp64_dedup[slot][i].sig == sig) return &lp64_dedup[slot][i];
    return 0;
}

static LP64_DEDUP* lp64_dedup_add(int slot, const void* bin, uint32_t sig)
{
    LP64_DEDUP* e;
    if (lp64_dedup_n[slot] >= LP64_DEDUP_MAX)
        return 0;   /* full: this unpack is simply not shared (correct, only the arena pays); never fatal */
    e = &lp64_dedup[slot][lp64_dedup_n[slot]++];
    e->bin = bin;
    e->sig = sig;
    return e;
}

static uint32_t lp64_fnv(uint32_t h, const unsigned char* p, size_t n)
{
    while (n--) { h ^= *p++; h *= 16777619u; }
    return h;
}

void lp64_reset(int slot)
{
    lp64_arena_init();
    if ((unsigned)slot >= LP64_SLOT_COUNT) return;
    lp64_bump[slot] = lp64_base[slot];
    lp64_dedup_n[slot] = 0;
}

void lp64_arena_bounds(int slot, const void** lo, const void** hi)
{
    lp64_arena_init();
    *lo = lp64_base[slot];
    *hi = lp64_limit[slot];
}

int lp64_is_on(void) { return 1; }

static size_t lp64_slot_free(int slot) { lp64_arena_init(); return (size_t)(lp64_limit[slot] - lp64_bump[slot]); }

#else /* 32-bit host: zero-cost pass-through */

void lp64_reset(int slot) { (void)slot; }
void lp64_arena_bounds(int slot, const void** lo, const void** hi) { (void)slot; *lo = 0; *hi = 0; }
int  lp64_is_on(void) { return 0; }
static size_t lp64_slot_free(int slot) { (void)slot; return (size_t)-1; }

#endif

/* ------------------------------------------------------- disc word helpers */

#define rd32(p)      lp64_rdu32((const unsigned char*)(p))
#define rd16(p)       ((uint16_t)((const unsigned char*)(p))[0] | ((uint16_t)((const unsigned char*)(p))[1] << 8))
/* a disc float: the bits preserved, never value-cast */
static inline float lp64_rdfloat(const unsigned char* p) { float f; memcpy(&f, p, 4); return f; }
#define rdf(p)       lp64_rdfloat((const unsigned char*)(p))
/* The disc's 32-bit offsets are relative to a window address (< 4 GiB), so the
   relocation is exact in uintptr_t. */
#define reloff(base, v)   ((void*)(uintptr_t)((uintptr_t)(base) + (uint32_t)(v)))
#define rawsent(v)        ((void*)(uintptr_t)(uint32_t)(v))

/* 32-bit disc strides of the Ninja structures (fixed by the disc format).
   Note Angle is Sint32 (ninjadef.h), so NJS_CNK_OBJECT's ang[3] is 12 bytes. */
#define LP64_CNKOBJ_DISC  52u   /* NJS_CNK_OBJECT */
#define LP64_CNKMDL_DISC  24u   /* NJS_CNK_MODEL  */
#define LP64_MODEL_DISC   40u   /* NJS_MODEL      */
#define LP64_MESHSET_DISC 24u   /* NJS_MESHSET    */
#define LP64_TEXNAME_DISC 12u   /* NJS_TEXNAME    */
#define LP64_MOTION_DISC  12u   /* NJS_MOTION     */
#define LP64_MDATA2_DISC  8u    /* NJS_MDATA2_MOD */

/* ------------------------------------------------------------- room unpack */

#if ASHFORD_LP64

static int lp64_room_active;
static const unsigned char* lp64_evt_disc;   /* the disc EVT block (word array) */

static CUT_WORK* lp64_cut_unpack(const unsigned char* disc, unsigned count, const unsigned char* rdtp)
{
    CUT_WORK* arr;
    unsigned i;
    if (!count) return NULL;
    arr = (CUT_WORK*)lp64_alloc(LP64_SLOT_ROOM, count * sizeof(CUT_WORK), 8);
    for (i = 0; i < count; i++, disc += 0x2A8) {
        CUT_WORK* d = arr + i;
        d->flg = disc[0];
        d->type = disc[1];
        d->flr_no = disc[2];
        d->ctab_n = disc[3];
        d->cuttp = (CUT_WRK*)reloff(rdtp, rd32(disc + 4));   /* &rdtp[byte offset] */
        memcpy(&d->cx, disc + 8, 0x2A0);   /* cx..cd, cam[2], exd[256]: no pointers */
    }
    return arr;
}

static LGT_WORK* lp64_lgt_unpack(const unsigned char* disc, unsigned count)
{
    LGT_WORK* arr;
    unsigned i;
    if (!count) return NULL;
    arr = (LGT_WORK*)lp64_alloc(LP64_SLOT_ROOM, count * sizeof(LGT_WORK), 8);
    for (i = 0; i < count; i++, disc += 0xE0) {
        LGT_WORK* d = arr + i;
        memcpy(d, disc, 0xD4);   /* scalars up to waz */
        d->lkwkp = NULL;         /* runtime scratch (light.c writes it) */
        d->exp = NULL;
        d->light = NULL;
    }
    return arr;
}

static EVC_WORK* lp64_evc_unpack(const unsigned char* disc, unsigned count)
{
    EVC_WORK* arr;
    unsigned i, k;
    if (!count) return NULL;
    arr = (EVC_WORK*)lp64_alloc(LP64_SLOT_ROOM, count * sizeof(EVC_WORK), 8);
    for (i = 0; i < count; i++, disc += 0x808) {
        EVC_WORK* d = arr + i;
        d->flg = rd16(disc);
        d->type = rd16(disc + 2);
        d->nxt_no = (short)rd16(disc + 4);
        d->keyf_n = (short)rd16(disc + 6);
        for (k = 0; k < 16; k++) {
            CAM_KEYF_WORK* kf = &d->keyf[k];
            const unsigned char* s = disc + 8 + k * 0x80;
            memcpy(kf, s, 0x7C);
            kf->recp = NULL;   /* never read by the game */
        }
    }
    return arr;
}

int lp64_room_rom(ROM_WORK* rom, const unsigned char* rdtp)
{
    const unsigned char* disc = rdtp + rd32(rdtp + 16);   /* &rdtp[byte offset] */
    unsigned cut_n, lgt_n, evc_n;
    unsigned i;

    lp64_reset(LP64_SLOT_ROOM);   /* per-room: models, CUT/LGT/EVC arrays, EVT view */
    lp64_room_active = 1;

    /* The 16 leading pointer words, relocated unconditionally as the decomp's
       word loop does. */
    rom->cutp = (CUT_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_CUTP));
    rom->lgtp = (LGT_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_LGTP));
    rom->enep = (ETTY_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_ENEP));
    rom->objp = (ETTY_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_OBJP));
    rom->itmp = (ETTY_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_ITMP));
    rom->effp = (EF_WRK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_EFFP));
    rom->walp = (ATR_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_WALP));
    rom->etcp = (ATR_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_ETCP));
    rom->flrp = (ATR_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_FLRP));
    rom->posp = (POS*)reloff(rdtp, rd32(disc + LP64_RO_OFF_POSP));
    rom->rutp = (ATR_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_RUTP));
    rom->ruttp = (unsigned char*)reloff(rdtp, rd32(disc + LP64_RO_OFF_RUTTP));
    rom->evtp = (EVT_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_EVTP));
    rom->evcp = (EVC_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_EVCP));
    rom->mesp = (unsigned int*)reloff(rdtp, rd32(disc + LP64_RO_OFF_MESP));
    rom->evlp = (LGT_WORK*)reloff(rdtp, rd32(disc + LP64_RO_OFF_EVLP));
    for (i = 0; i < 16; i++)   /* dmp00..dmp15: raw disc values, like the copy */
        ((unsigned char**)&rom->dmp00)[i] = (unsigned char*)(uintptr_t)rd32(disc + 0x40 + 4 * i);

    rom->cut_n = (int)rd32(disc + LP64_RO_OFF_CUT_N);
    rom->lgt_n = (int)rd32(disc + LP64_RO_OFF_LGT_N);
    rom->ene_n = (int)rd32(disc + LP64_RO_OFF_ENE_N);
    rom->obj_n = (int)rd32(disc + LP64_RO_OFF_OBJ_N);
    rom->itm_n = (int)rd32(disc + LP64_RO_OFF_ITM_N);
    rom->eff_n = (int)rd32(disc + LP64_RO_OFF_EFF_N);
    rom->wal_n = (int)rd32(disc + 0x98);
    rom->etc_n = (int)rd32(disc + 0x9C);
    rom->flr_n = (int)rd32(disc + 0xA0);
    rom->pos_n = (int)rd32(disc + 0xA4);
    rom->rut_n = (int)rd32(disc + 0xA8);
    rom->evc_n = (int)rd32(disc + LP64_RO_OFF_EVC_N);
    rom->mes_n = (int)rd32(disc + LP64_RO_OFF_MES_N);
    rom->evl_n = (int)rd32(disc + LP64_RO_OFF_EVL_N);
    memcpy(&rom->dmy00, disc + 0xB8, 0x48);   /* dmy00..dmy17 */

    rom->flg = rd32(disc + 0x100);
    rom->bak_col = rd32(disc + 0x104);
    rom->fog_col = rd32(disc + 0x108);
    rom->fog_nr = rdf(disc + 0x10C);
    rom->fog_fr = rdf(disc + 0x110);
    rom->w = rdf(disc + 0x114);
    rom->h = rdf(disc + 0x118);
    rom->d = rdf(disc + 0x11C);
    memcpy(rom->grand, disc + 0x120, sizeof(rom->grand));
    memset(&rom->mdl, 0, sizeof(rom->mdl));   /* bhSetRoom zeroes it before bhMlbBinRealize */
    memcpy(rom->fog, disc + 0x1B8, sizeof(rom->fog));
    rom->amb_rom = (char)disc[0x3B8];
    rom->amb_chr = (char)disc[0x3B9];
    rom->amb_obj = (char)disc[0x3BA];
    rom->amb_itm = (char)disc[0x3BB];
    memcpy(rom->amb_r, disc + 0x3BC, sizeof(rom->amb_r));
    memcpy(rom->amb_g, disc + 0x3CC, sizeof(rom->amb_g));
    memcpy(rom->amb_b, disc + 0x3DC, sizeof(rom->amb_b));

    cut_n = (unsigned)rom->cut_n;
    lgt_n = (unsigned)rom->lgt_n;
    evc_n = (unsigned)rom->evc_n;
    if (cut_n < 0x1000u) rom->cutp = lp64_cut_unpack((const unsigned char*)rom->cutp, cut_n, rdtp);
    if (lgt_n < 0x1000u) {
        rom->lgtp = lp64_lgt_unpack((const unsigned char*)rom->lgtp, lgt_n);
        /* evlp: the same LGT_WORK layout, indexed [i - 4] by light.c */
        rom->evlp = lp64_lgt_unpack((const unsigned char*)rom->evlp, lgt_n);
    }
    if (evc_n < 0x1000u) rom->evcp = lp64_evc_unpack((const unsigned char*)rom->evcp, evc_n);

    /* EVT block: bhSetRoom (room.c) overwrites rom->evtp after the ROM_WORK copy with
       &rdtp[word at rdtp + 28]; the ROM_WORK's own evtp word is not the EVT block (on retail rooms
       it points elsewhere). The script interpreter, Event_init and the glue (biorand, cast, cheat,
       enemyopt) all treat rom->evtp as the block's base address: its first three words {scd0,
       scd1, evd} are byte offsets from it (read as 32-bit words by bhInitEvent on LP64, where
       EVT_WORK's fields are pointer-sized), the words after them per-event offsets (Event_init). */
    lp64_evt_disc = (const unsigned char*)reloff(rdtp, rd32(rdtp + 28));
    rom->evtp = (EVT_WORK*)lp64_evt_disc;
    return 1;
}

int lp64_active(void) { return lp64_room_active; }

ptrdiff_t lp64_evt_offset(unsigned evt_id)
{
    /* the same word `((int*)rom->evtp)[evt_id + (sizeof(EVT_WORK)/4 - 1)]` reads */
    return (ptrdiff_t)(int32_t)rd32(lp64_evt_disc + (2u + evt_id) * 4u);
}

#else /* 32-bit host */

int lp64_room_rom(ROM_WORK* rom, const unsigned char* rdtp) { (void)rom; (void)rdtp; return 0; }
int lp64_active(void) { return 0; }
ptrdiff_t lp64_evt_offset(unsigned evt_id)
{
    /* identical to the original expression */
    return (ptrdiff_t)((int*)rom->evtp)[evt_id + (sizeof(EVT_WORK) / 4) - 1];
}

#endif

/* ------------------------------------------------------------ Ninja trees */

#if ASHFORD_LP64

static NJS_CNK_MODEL* lp64_cnk_model_unpack(const unsigned char* mdisc, const unsigned char* dat, int slot)
{
    NJS_CNK_MODEL* m = (NJS_CNK_MODEL*)lp64_alloc(slot, sizeof(NJS_CNK_MODEL), 8);
    float r;
    memcpy(&r, mdisc + 20, 4);
    memcpy(&m->center, mdisc + 8, 12);
    if (r < 0) {
        uint32_t v;
        m->r = -r;
        v = rd32(mdisc);
        m->vlist = (v == 0xFFFFFFFFu) ? (Sint32*)rawsent(v) : (Sint32*)reloff(dat, v);
        v = rd32(mdisc + 4);
        m->plist = (v == 0xFFFFFFFFu) ? (Sint16*)rawsent(v) : (Sint16*)reloff(dat, v);
    } else {
        m->r = r;
        m->vlist = (Sint32*)(uintptr_t)rd32(mdisc);
        m->plist = (Sint16*)(uintptr_t)rd32(mdisc + 4);
    }
    return m;
}

static NJS_MESHSET* lp64_meshsets_unpack(const unsigned char* mdisc, unsigned nb, const unsigned char* dat, int slot)
{
    NJS_MESHSET* arr = (NJS_MESHSET*)lp64_alloc(slot, nb * sizeof(NJS_MESHSET), 8);
    unsigned i;
    for (i = 0; i < nb; i++, mdisc += LP64_MESHSET_DISC) {
        NJS_MESHSET* d = arr + i;
        uint32_t v;
        d->type_matId = rd16(mdisc);
        d->nbMesh = rd16(mdisc + 2);
        v = rd32(mdisc + 4);
        d->meshes = (v == 0xFFFFFFFFu) ? (Sint16*)rawsent(v) : (Sint16*)reloff(dat, v);   /* no NULL branch in the decomp */
        v = rd32(mdisc + 8);
        d->attrs = (v == 0xFFFFFFFFu) ? NULL : (uint32_t*)reloff(dat, v);
        v = rd32(mdisc + 12);
        d->normals = (v == 0xFFFFFFFFu) ? NULL : (NJS_VECTOR*)reloff(dat, v);
        v = rd32(mdisc + 16);
        d->vertcolor = (v == 0xFFFFFFFFu) ? NULL : (NJS_COLOR*)reloff(dat, v);
        v = rd32(mdisc + 20);
        d->vertuv = (v == 0xFFFFFFFFu) ? NULL : (NJS_COLOR*)reloff(dat, v);
    }
    return arr;
}

static NJS_MODEL* lp64_bsc_model_unpack(const unsigned char* mdisc, const unsigned char* dat, int slot)
{
    NJS_MODEL* m = (NJS_MODEL*)lp64_alloc(slot, sizeof(NJS_MODEL), 8);
    uint32_t nbPoint = rd32(mdisc + 8);
    uint32_t v;
    m->nbMeshset = rd16(mdisc + 20);
    m->nbMat = rd16(mdisc + 22);
    if (nbPoint & 0x80000000u) {
        m->nbPoint = (Sint32)(nbPoint & ~0x80000000u);
        v = rd32(mdisc);
        m->points = (v == 0xFFFFFFFFu) ? (NJS_POINT3*)rawsent(v) : (NJS_POINT3*)reloff(dat, v);
        v = rd32(mdisc + 4);
        m->normals = (v == 0xFFFFFFFFu) ? (NJS_VECTOR*)rawsent(v) : (NJS_VECTOR*)reloff(dat, v);
        v = rd32(mdisc + 12);
        if (v == 0xFFFFFFFFu) m->meshsets = (NJS_MESHSET*)rawsent(v);
        else m->meshsets = lp64_meshsets_unpack(dat + v, m->nbMeshset, dat, slot);
        v = rd32(mdisc + 16);
        m->mats = (v == 0xFFFFFFFFu) ? (NJS_MATERIAL*)rawsent(v) : (NJS_MATERIAL*)reloff(dat, v);
    } else {
        m->points = (NJS_POINT3*)(uintptr_t)rd32(mdisc);
        m->normals = (NJS_VECTOR*)(uintptr_t)rd32(mdisc + 4);
        m->nbPoint = (Sint32)nbPoint;
        m->meshsets = (NJS_MESHSET*)(uintptr_t)rd32(mdisc + 12);
        m->mats = (NJS_MATERIAL*)(uintptr_t)rd32(mdisc + 16);
    }
    memcpy(&m->center, mdisc + 24, 12);
    memcpy(&m->r, mdisc + 36, 4);
    return m;
}

/* What an ML unpack is built from: the 16-byte header, the object table, each object's model header
   and the texture table. The unpacked trees are fully determined by these bytes (vertex / strip data
   stay in the disc buffer, reached by pointer), so equal signatures mean an equal unpack. */
static uint32_t lp64_ml_sig(const unsigned char* bin)
{
    uint32_t h = 2166136261u;
    uint32_t status = bin[3], obj_num = rd16(bin + 6), tex_off = rd32(bin + 8), obj_off = rd32(bin + 12);
    const unsigned char* dat = bin + rd16(bin + 4);
    h = lp64_fnv(h, bin, 16);
    if (obj_off != 0xFFFFFFFFu) {
        uint32_t i;
        h = lp64_fnv(h, dat + obj_off, (size_t)obj_num * LP64_CNKOBJ_DISC);
        for (i = 0; i < obj_num; i++) {
            uint32_t v = rd32(dat + obj_off + i * LP64_CNKOBJ_DISC + 4);
            if (v != 0xFFFFFFFFu) h = lp64_fnv(h, dat + v, (status & 0x80) ? LP64_CNKMDL_DISC : LP64_MODEL_DISC);
        }
    }
    if (tex_off != 0xFFFFFFFFu) {
        const unsigned char* tdisc = dat + tex_off;
        uint32_t nb = rd32(tdisc + 4);
        h = lp64_fnv(h, tdisc, 8);
        h = lp64_fnv(h, dat + rd32(tdisc), (size_t)nb * LP64_TEXNAME_DISC);
    }
    return h;
}

/* The same for an MN motion: header, NJS_MOTION record and the MDATA2 table. */
static uint32_t lp64_mn_sig(const unsigned char* bin)
{
    uint32_t h = 2166136261u, n = rd16(bin + 6);
    const unsigned char* dat_top = bin + rd16(bin + 4);
    const unsigned char* mtn_disc = dat_top + rd32(bin + 8);
    h = lp64_fnv(h, bin, 16);
    h = lp64_fnv(h, mtn_disc, LP64_MOTION_DISC);
    h = lp64_fnv(h, dat_top + rd32(mtn_disc), (size_t)n * LP64_MDATA2_DISC);
    return h;
}

int lp64_mlb(const void* bin_datP, ML_WORK* mlwP, int slot)
{
    const unsigned char* bin = (const unsigned char*)bin_datP;
    const unsigned char* dat;
    uint32_t tex_off, obj_off;
    uint32_t obj_num, status;
    const LP64_DEDUP* hit;
    uint32_t sig = lp64_ml_sig(bin);

    status = bin[3];
    obj_num = rd16(bin + 6);
    tex_off = rd32(bin + 8);
    obj_off = rd32(bin + 12);
    dat = bin + rd16(bin + 4);

    hit = lp64_dedup_find(slot, bin, sig);
    if (hit) {
        mlwP->flg = status;
        mlwP->obj_num = (obj_off != 0xFFFFFFFFu) ? obj_num : 0;
        mlwP->datP = (void*)dat;
        mlwP->objP = (NJS_CNK_OBJECT*)hit->objP;
        mlwP->texP = (NJS_TEXLIST*)hit->texP;
        mlwP->owP = NULL;
        return 1;
    }

    mlwP->flg = 0;
    mlwP->obj_num = 0;
    mlwP->datP = (void*)dat;
    mlwP->objP = NULL;
    mlwP->texP = NULL;
    mlwP->owP = NULL;

    if (obj_off != 0xFFFFFFFFu) {
        NJS_CNK_OBJECT* arr = (NJS_CNK_OBJECT*)lp64_alloc(slot, obj_num * sizeof(NJS_CNK_OBJECT), 8);
        uint32_t i;
        mlwP->flg = status;
        mlwP->obj_num = obj_num;
        for (i = 0; i < obj_num; i++) {
            const unsigned char* src = dat + obj_off + i * LP64_CNKOBJ_DISC;
            NJS_CNK_OBJECT* d = arr + i;
            uint32_t v;
            d->evalflags = rd32(src);
            v = rd32(src + 4);
            if (v == 0xFFFFFFFFu) d->model = NULL;
            else d->model = (status & 0x80)
                ? (NJS_CNK_MODEL*)lp64_cnk_model_unpack(dat + v, dat, slot)
                : (NJS_CNK_MODEL*)lp64_bsc_model_unpack(dat + v, dat, slot);
            memcpy(d->pos, src + 8, 12);
            memcpy(d->ang, src + 20, 12);   /* Angle is Sint32: 3 x 4 */
            memcpy(d->scl, src + 32, 12);
            v = rd32(src + 44);
            d->child = (v == 0xFFFFFFFFu) ? NULL : (struct cnkobj*)(arr + (v - obj_off) / LP64_CNKOBJ_DISC);
            v = rd32(src + 48);
            d->sibling = (v == 0xFFFFFFFFu) ? NULL : (struct cnkobj*)(arr + (v - obj_off) / LP64_CNKOBJ_DISC);
        }
        mlwP->objP = arr;
    }

    if (tex_off != 0xFFFFFFFFu) {
        const unsigned char* tdisc = dat + tex_off;
        uint32_t nb = rd32(tdisc + 4);
        NJS_TEXLIST* tl = (NJS_TEXLIST*)lp64_alloc(slot, sizeof(NJS_TEXLIST), 8);
        NJS_TEXNAME* names = (NJS_TEXNAME*)lp64_alloc(slot, (nb ? nb : 1) * sizeof(NJS_TEXNAME), 8);
        uint32_t i;
        tl->textures = names;
        tl->nbTexture = nb;
        for (i = 0; i < nb; i++) {
            const unsigned char* n = dat + rd32(tdisc) + i * LP64_TEXNAME_DISC;
            names[i].filename = reloff(dat, rd32(n));   /* unconditional, like the decomp */
            names[i].attr = rd32(n + 4);
            names[i].texaddr = rd32(n + 8);
        }
        mlwP->texP = tl;
    }

    {
        LP64_DEDUP* e = lp64_dedup_add(slot, bin, sig);
        if (e) {
            e->objP = mlwP->objP;
            e->texP = mlwP->texP;
            e->md2P = 0;
        }
    }
    return 1;
}

int lp64_mnb(const void* bin_datP, MN_WORK* mnwP, int slot)
{
    const unsigned char* bin = (const unsigned char*)bin_datP;
    const unsigned char* dat_top = bin + rd16(bin + 4);
    const unsigned char* mtn_disc = dat_top + rd32(bin + 8);
    const unsigned char* md2_disc = dat_top + rd32(mtn_disc);
    uint32_t n = rd16(bin + 6);
    uint32_t atr = rd32(bin + 12);
    NJS_MDATA2_MOD* md2;
    uint32_t i, sig;

    sig = lp64_mn_sig(bin);
    {
        const LP64_DEDUP* hit = lp64_dedup_find(slot, bin, sig);
        if (hit && hit->md2P) {
            mnwP->flg = bin[3];
            mnwP->obj_num = n;
            mnwP->frm_num = rd32(mtn_disc + 4);
            mnwP->datP = (void*)bin;
            mnwP->md2P = (NJS_MDATA2_MOD*)hit->md2P;
            mnwP->atrP = (atr != 0xFFFFFFFFu) ? (unsigned short*)reloff(dat_top, atr) : NULL;
            return 1;
        }
    }

    md2 = (NJS_MDATA2_MOD*)lp64_alloc(slot, (n ? n : 1) * sizeof(NJS_MDATA2_MOD), 8);
    for (i = 0; i < n; i++) {
        uint32_t p0 = rd32(md2_disc + i * LP64_MDATA2_DISC);
        uint32_t p1 = rd32(md2_disc + i * LP64_MDATA2_DISC + 4);
        md2[i].p[0] = (p0 == 0xFFFFFFFFu) ? NULL : reloff(dat_top, p0);
        md2[i].p[1] = (p1 == 0xFFFFFFFFu) ? NULL : reloff(dat_top, p1);
    }
    mnwP->flg = bin[3];
    mnwP->obj_num = n;
    mnwP->frm_num = rd32(mtn_disc + 4);   /* NJS_MOTION.nbFrame */
    mnwP->datP = (void*)bin;
    mnwP->md2P = md2;
    mnwP->atrP = (atr != 0xFFFFFFFFu) ? (unsigned short*)reloff(dat_top, atr) : NULL;

    {
        LP64_DEDUP* e = lp64_dedup_add(slot, bin, sig);
        if (e) {
            e->objP = 0;
            e->texP = 0;
            e->md2P = md2;
        }
    }
    return 1;
}

#else /* 32-bit host */

int lp64_mlb(const void* bin_datP, ML_WORK* mlwP, int slot) { (void)bin_datP; (void)mlwP; (void)slot; return 0; }
int lp64_mnb(const void* bin_datP, MN_WORK* mnwP, int slot) { (void)bin_datP; (void)mnwP; (void)slot; return 0; }

#endif

/* Glue-side (model swap, enemy variants, placed objects) callers realize models whose number is not
   bounded by the room or the player, so they cannot afford the arena's fatal exhaustion: below
   LP64_SOFT_MIN free bytes a model that is not already unpacked is left empty (objP NULL: nothing
   loaded, which the callers handle) instead of aborting. A deduplicated model costs nothing and is
   always served. 32-bit hosts: 0, the caller runs the original realizer. */
#if ASHFORD_LP64
#define LP64_SOFT_MIN (2u << 20)
int lp64_mlb_soft(const void* bin_datP, ML_WORK* mlwP, int slot)
{
    if (lp64_slot_free(slot) < LP64_SOFT_MIN && !lp64_dedup_find(slot, bin_datP, lp64_ml_sig((const unsigned char*)bin_datP))) {
        host_log("lp64: arena slot %d nearly full (%zu bytes free): model not loaded\n", slot, lp64_slot_free(slot));
        memset(mlwP, 0, sizeof(*mlwP));
        return 1;
    }
    return lp64_mlb(bin_datP, mlwP, slot);
}
int lp64_mnb_soft(const void* bin_datP, MN_WORK* mnwP, int slot)
{
    if (lp64_slot_free(slot) < LP64_SOFT_MIN) {
        host_log("lp64: arena slot %d nearly full (%zu bytes free): motion not loaded\n", slot, lp64_slot_free(slot));
        memset(mnwP, 0, sizeof(*mnwP));
        return 1;
    }
    return lp64_mnb(bin_datP, mnwP, slot);
}
#else
int lp64_mlb_soft(const void* bin_datP, ML_WORK* mlwP, int slot) { (void)bin_datP; (void)mlwP; (void)slot; return 0; }
int lp64_mnb_soft(const void* bin_datP, MN_WORK* mnwP, int slot) { (void)bin_datP; (void)mnwP; (void)slot; return 0; }
#endif

/* -------------------------------------------- decomp-facing hook wrappers */

int pc_lp64_room_rom(ROM_WORK* rom, const unsigned char* rdtp) { return lp64_room_rom(rom, rdtp); }
int pc_lp64_active(void) { return lp64_active(); }
ptrdiff_t pc_lp64_evt_offset(unsigned evt_id) { return lp64_evt_offset(evt_id); }
int pc_lp64_mlb_room(void* bin_datP, ML_WORK* mlwP) { return lp64_mlb(bin_datP, mlwP, LP64_SLOT_ROOM); }
int pc_lp64_mlb_session(void* bin_datP, ML_WORK* mlwP) { return lp64_mlb(bin_datP, mlwP, LP64_SLOT_SESSION); }
int pc_lp64_mnb_room(void* bin_datP, MN_WORK* mnwP) { return lp64_mnb(bin_datP, mnwP, LP64_SLOT_ROOM); }
int pc_lp64_mnb_session(void* bin_datP, MN_WORK* mnwP) { return lp64_mnb(bin_datP, mnwP, LP64_SLOT_SESSION); }
void pc_lp64_room_reset(void) { lp64_reset(LP64_SLOT_ROOM); }

