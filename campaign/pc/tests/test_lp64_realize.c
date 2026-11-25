/* LP64 on-disc binary struct realization (pc/glue/lp64_realize.{h,c}).
 *
 * Synthetic 32-bit PS2-style disc fixtures, hand-packed, for every unpacked
 * structure: ROM_WORK, CUT_WORK, LGT_WORK, EVC_WORK, EVT_WORK, the Ninja
 * trees (NJS_CNK_OBJECT, NJS_CNK_MODEL, NJS_MODEL, NJS_MESHSET,
 * NJS_TEXLIST, NJS_TEXNAME), and NJS_MOTION / NJS_MDATA2_MOD — plus
 * 0xFFFFFFFF sentinels, shared-model dedup, arena reset, arena bounds, and
 * the pristine-disc guarantee (the unpack never writes disc bytes).
 *
 * On 32-bit hosts (Windows x86) the LP64 code compiles away, and the same
 * fixtures verify the pass-through: every hook reports "not handled" and
 * the disc stays untouched, so the decomp's original in-place path runs.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "lp64_realize.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static uint32_t packf(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static void w32(unsigned char* p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }
static void w16(unsigned char* p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void wf(unsigned char* p, float f) { w32(p, packf(f)); }

/* The symbols lp64_realize.c's host-facing stubs reference. The host layer
   (pc/src/host/log.c, the game's romp) is not linked into this test. */
ROM_WORK* rom;
void host_fatal(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

/* ------------------------------------------------------- ROM_WORK fixture */

/* A synthetic room file: RDT header words, the 32-bit ROM_WORK at byte
   offset ROMW, then the CUT/LGT/EVC arrays and the EVT block. */
static unsigned char room_file[0x8000];
#define ROMW      0x40u   /* byte offset of the ROM_WORK inside room_file (word-aligned) */
#define CUT_REL   0x480u  /* byte offsets the ROM's pointer words name (relative to room_file) */
#define LGT_REL   0x800u
#define EVL_REL   0xC00u
#define EVC_REL   0x1000u
#define EVT_REL   0x1900u
#define MESP_REL  0x2000u

static void build_room_file(void)
{
    memset(room_file, 0xAB, sizeof(room_file));
    wf(room_file + 0, 1.8f);          /* version */
    w32(room_file + 16, ROMW);        /* the ROM_WORK, a byte offset like &rdtp[val] */

    unsigned char* r = room_file + ROMW;
    /* the 16 leading pointer words: room-file-relative offsets */
    w32(r + 0x00, CUT_REL);
    w32(r + 0x04, LGT_REL);
    w32(r + 0x08, 0x30 * 4);   /* enep */
    w32(r + 0x0C, 0x31 * 4);   /* objp */
    w32(r + 0x10, 0x32 * 4);   /* itmp */
    w32(r + 0x14, 0x33 * 4);   /* effp */
    w32(r + 0x18, 0x34 * 4);   /* walp */
    w32(r + 0x1C, 0x35 * 4);   /* etcp */
    w32(r + 0x20, 0x36 * 4);   /* flrp */
    w32(r + 0x24, 0x37 * 4);   /* posp */
    w32(r + 0x28, 0x38 * 4);   /* rutp */
    w32(r + 0x2C, 0x39 * 4);   /* ruttp */
    w32(r + 0x30, EVT_REL);   /* evtp */
    w32(r + 0x34, EVC_REL);   /* evcp */
    w32(r + 0x38, MESP_REL);  /* mesp */
    w32(r + 0x3C, EVL_REL);   /* evlp */
    /* dmp00..dmp15: raw values, never relocated by the decomp's word loop */
    w32(r + 0x40 + 4 * 5, 0xD5D5D5D5u);
    w32(r + 0x40 + 4 * 13, 0x13131313u);
    /* counts */
    w32(r + 0x80, 2);          /* cut_n */
    w32(r + 0x84, 3);          /* lgt_n */
    w32(r + 0x88, 4);          /* ene_n */
    w32(r + 0x8C, 6);          /* obj_n */
    w32(r + 0x90, 1);          /* itm_n */
    w32(r + 0x94, 2);          /* eff_n */
    w32(r + 0x98, 1);          /* wal_n */
    w32(r + 0x9C, 1);          /* etc_n */
    w32(r + 0xA0, 1);          /* flr_n */
    w32(r + 0xA4, 1);          /* pos_n */
    w32(r + 0xA8, 1);          /* rut_n */
    w32(r + 0xAC, 1);          /* evc_n */
    w32(r + 0xB0, 7);          /* mes_n */
    w32(r + 0xB4, 1);          /* evl_n */
    w32(r + 0xB8 + 4 * 3, 0x03030303u);   /* dmy03 */
    w32(r + 0x100, 0x00000101u);   /* flg */
    w32(r + 0x104, 0x0000B0B0u);   /* bak_col */
    w32(r + 0x108, 0x00FF00FFu);   /* fog_col */
    wf(r + 0x10C, 4.5f);           /* fog_nr */
    wf(r + 0x110, 90.0f);          /* fog_fr */
    wf(r + 0x114, 1000.0f);        /* w */
    wf(r + 0x118, 2000.0f);        /* h */
    wf(r + 0x11C, 3000.0f);        /* d */
    wf(r + 0x120 + 4 * 31, 5.25f); /* grand[31] (bhSetRoom zeroes it after) */
    w32(r + 0x1A0 + 8, 0x77AA77AAu);  /* mdl disc bytes: dropped by the unpack */
    wf(r + 0x1B8 + 4 * 100, 6.75f);    /* fog[100] */
    r[0x3B8] = 1; r[0x3B9] = 2; r[0x3BA] = 3; r[0x3BB] = 4;
    wf(r + 0x3BC, 0.25f); wf(r + 0x3BC + 4 * 3, 0.5f);   /* amb_r[0], amb_r[3] */
    wf(r + 0x3DC, 0.125f);                               /* amb_b[0] */

    /* CUT_WORK array: 2 x 0x2A8 */
    {
        unsigned char* c = room_file + CUT_REL;
        w32(c + 4, 0x70);         /* cuttp as a byte offset, like &rdtp[val] */
        wf(c + 8, 11.0f);         /* cx */
        c[0] = 7; c[1] = 8; c[2] = 9; c[3] = 10;
        c += 0x2A8;
        c[0] = 1; c[1] = 2; c[2] = 3; c[3] = 10;
        w32(c + 4, 0x71);
        wf(c + 0x14, 12.0f);    /* cw */
    }
    /* LGT_WORK arrays: lgtp and evlp, 3 x 0xE0 each */
    {
        unsigned char* l = room_file + LGT_REL;
        wf(l + 4 * 3, 1.5f);            /* mode (runtime-written anyway) */
        w32(l + 0xD0, 0x777);             /* waz */
        w32(l + 0xD4, 0xEEEEEEEEu);      /* lkwkp disc value: scratch, zeroed */
        w32(l + 0xD8, 0xAAAAAAAAu);      /* exp */
        w32(l + 0xDC, 0xBBBBBBBBu);      /* light */
        memcpy(room_file + EVL_REL, l, 0xE0 * 3);
    }
    /* EVC_WORK array: 1 x 0x808 */
    {
        unsigned char* e = room_file + EVC_REL;
        w16(e + 0, 0x1234); w16(e + 2, 0x5678);
        w16(e + 4, 1); w16(e + 6, 16);   /* nxt_no, keyf_n */
        wf(e + 8 + 0x68, 8.5f);           /* keyf[0].prm_0 */
        w32(e + 8 + 0x7C, 0xCCCCCCCCu);  /* keyf[0].recp: never read, zeroed */
        w16(e + 8 + 0x80 + 2, 0x4242);    /* keyf[1].frame */
    }
    /* EVT block: scd0/scd1/evd as window words, then per-event offsets */
    {
        unsigned char* v = room_file + EVT_REL;
        w32(v + 0, 0x33000100u);
        w32(v + 4, 0x33000200u);
        w32(v + 8, 0x33000300u);
        w32(v + 12, 16);    /* event 0's offset (word 3) */
        w32(v + 16, -32);   /* event 1's offset: negative, sign preserved */
        w32(v + 20, 48);    /* event 2 */
    }
}

static int in_bounds(const void* p, int slot)
{
    const void *lo, *hi;
    lp64_arena_bounds(slot, &lo, &hi);
    return (const char*)p >= (const char*)lo && (const char*)p < (const char*)hi;
}

static int test_rom_work(void)
{
    static ROM_WORK rom;
    unsigned char before[sizeof(room_file)];
    build_room_file();
    memcpy(before, room_file, sizeof(room_file));

    if (lp64_is_on()) {
        CHECK(lp64_room_rom(&rom, room_file) == 1);
        CHECK(rom.cut_n == 2 && rom.lgt_n == 3 && rom.ene_n == 4 && rom.obj_n == 6);
        CHECK(rom.itm_n == 1 && rom.eff_n == 2 && rom.evc_n == 1 && rom.mes_n == 7 && rom.evl_n == 1);
        /* leading pointer words: relocated like the decomp's word loop */
        CHECK(rom.enep == (ETTY_WORK*)(room_file + 0x30 * 4));
        CHECK(rom.mesp == (unsigned int*)((uintptr_t)room_file + MESP_REL));
        /* dmp fields keep their raw disc values (the loop stops before dmp00) */
        CHECK((uintptr_t)rom.dmp05 == 0xD5D5D5D5u);
        CHECK((uintptr_t)rom.dmp13 == 0x13131313u);
        CHECK(rom.dmy03 == 0x03030303u);
        CHECK(rom.flg == 0x101 && rom.bak_col == 0xB0B0 && rom.fog_col == 0x00FF00FFu);
        CHECK(rom.fog_nr == 4.5f && rom.fog_fr == 90.0f);
        CHECK(rom.w == 1000.0f && rom.h == 2000.0f && rom.d == 3000.0f);
        { uint32_t g0; memcpy(&g0, &rom.grand[0], 4); CHECK(g0 == 0xABABABABu); }   /* scalars copied verbatim; grand[31] is what bhSetRoom zeroes */
        CHECK(rom.fog[100] == 6.75f);
        CHECK(rom.amb_rom == 1 && rom.amb_chr == 2 && rom.amb_obj == 3 && rom.amb_itm == 4);
        CHECK(rom.amb_r[0] == 0.25f);
        CHECK(rom.amb_r[3] == 0.5f);
        CHECK(rom.amb_b[0] == 0.125f);
        CHECK(rom.mdl.flg == 0 && rom.mdl.objP == 0);   /* zeroed ahead of bhMlbBinRealize */

        /* the unpacked CUT array */
        CHECK(in_bounds(rom.cutp, LP64_SLOT_ROOM));
        CHECK(rom.cutp[0].flg == 7 && rom.cutp[0].type == 8 && rom.cutp[1].ctab_n == 10);
        CHECK(rom.cutp[0].cuttp == (CUT_WRK*)(room_file + 0x70));
        CHECK(rom.cutp[0].cx == 11.0f && rom.cutp[1].cw == 12.0f);

        /* the unpacked LGT arrays (lgtp and evlp): scalars copied, scratch zeroed */
        CHECK(in_bounds(rom.lgtp, LP64_SLOT_ROOM) && in_bounds(rom.evlp, LP64_SLOT_ROOM));
        CHECK(rom.lgtp[0].waz == 0x777);
        CHECK(rom.lgtp[0].lkwkp == NULL && rom.lgtp[0].exp == NULL && rom.lgtp[0].light == NULL);
        CHECK(rom.lgtp[2].lkwkp == NULL && rom.lgtp[2].light == NULL);
        CHECK(rom.evlp[0].waz == 0x777 && rom.evlp[1].light == NULL);

        /* the unpacked EVC array: scalars copied, the unused recp zeroed */
        CHECK(in_bounds(rom.evcp, LP64_SLOT_ROOM));
        CHECK(rom.evcp[0].flg == 0x1234 && rom.evcp[0].type == 0x5678);
        CHECK(rom.evcp[0].keyf_n == 16);
        CHECK(rom.evcp[0].keyf[0].prm_0 == 8.5f);
        CHECK(rom.evcp[0].keyf[0].recp == NULL);
        CHECK(rom.evcp[0].keyf[1].frame == 0x4242);

        /* the EVT view: the three words zero-extended, offsets by index */
        CHECK(in_bounds(rom.evtp, LP64_SLOT_ROOM));
        CHECK((uintptr_t)rom.evtp->scd0 == 0x33000100u);
        CHECK((uintptr_t)rom.evtp->scd1 == 0x33000200u);
        CHECK((uintptr_t)rom.evtp->evd == 0x33000300u);
        CHECK(lp64_evt_offset(0) == (ptrdiff_t)(int32_t)0x33000300u);   /* the decomp reads word[evt_id+2]: event 0's offset IS the evd word */
        CHECK(lp64_evt_offset(1) == 16);
        CHECK(lp64_evt_offset(2) == -32);
        CHECK(lp64_evt_offset(3) == 48);
        CHECK(lp64_active() == 1);
    } else {
        CHECK(lp64_room_rom(&rom, room_file) == 0);
        CHECK(lp64_active() == 0);
    }
    /* pristine disc: the unpack never writes disc bytes */
    CHECK(memcmp(before, room_file, sizeof(room_file)) == 0);
    printf("lp64 ROM_WORK/CUT/LGT/EVC/EVT: ok (%s)\n", lp64_is_on() ? "LP64" : "32-bit pass-through");
    return 0;
}

/* ------------------------------------------------------- Ninja ML fixture */

/* A synthetic model binary in the disc layout binfunc.c reads:
   bytes 0-3 header, byte 3 status, ushort[2] header size, ushort[3] obj_num,
   word[2] tex_off, word[3] obj_off, then dat = bin + header size. */
static unsigned char mlb_file[0x2000];
#define MLB_HDR  0x10u
#define MLB_OBJ  0x40u    /* obj_off: the CNK object array inside dat */
#define MLB_TEX  0x100u   /* tex_off: the texlist inside dat */
#define MLB_MDL  0x180u   /* the CNK model inside dat */

static void wstr(unsigned char* p, const char* s) { while (*s) *p++ = (unsigned char)*s++; *p = 0; }

static void build_mlb_file(unsigned char* file)
{
    memset(file, 0xCD, 0x2000);
    file[3] = 0x80 | 2;              /* status: CNK models, two objects */
    w16(file + 4, MLB_HDR);          /* ushort[2]: the header size */
    w16(file + 6, 2);                /* ushort[3]: obj_num */
    w32(file + 8, MLB_TEX);          /* tex_off */
    w32(file + 12, MLB_OBJ);         /* obj_off */
    {
        unsigned char* dat = file + MLB_HDR;
        /* CNK object 0: model -> the CNK model; child NULL; sibling -> entry 1 */
        unsigned char* o = dat + MLB_OBJ;
        w32(o + 0, 0x00000011u);                 /* evalflags */
        w32(o + 4, MLB_MDL);                      /* model (dat-relative) */
        wf(o + 8, 1.0f); wf(o + 12, 2.0f); wf(o + 16, 3.0f);   /* pos */
        w32(o + 20, 100); w32(o + 24, 200); w32(o + 28, 300);  /* ang (Angle = Sint32) */
        wf(o + 32, 4.0f); wf(o + 36, 5.0f); wf(o + 40, 6.0f); /* scl */
        w32(o + 44, 0xFFFFFFFFu);                 /* child: sentinel -> NULL */
        w32(o + 48, MLB_OBJ + 52);                /* sibling: entry 1 */
        /* CNK object 1: model sentinel -> NULL, child -> entry 0 */
        o += 52;
        w32(o + 0, 0x00000022u);
        w32(o + 4, 0xFFFFFFFFu);
        w32(o + 44, MLB_OBJ);                     /* child: entry 0 */
        w32(o + 48, 0xFFFFFFFFu);
        /* the CNK model: vlist/plist dat-relative, r negative (unrealized) */
        {
            unsigned char* m = dat + MLB_MDL;
            w32(m + 0, 0x400);
            w32(m + 4, 0x500);
            wf(m + 8, 1.25f); wf(m + 12, 2.25f); wf(m + 16, 3.25f);   /* center */
            wf(m + 20, -6.5f);                                        /* r */
        }
        /* the texlist and its two texnames */
        {
            unsigned char* t = dat + MLB_TEX;
            w32(t + 0, 0x600);       /* textures (dat-relative) */
            w32(t + 4, 2);            /* nbTexture */
            {
                unsigned char* n = dat + 0x600;
                w32(n + 0, 0x700); w32(n + 4, 0x00110011u); w32(n + 8, 0x00C0FFEEu);
                n += 12;
                w32(n + 0, 0x708); w32(n + 4, 0x00220022u); w32(n + 8, 0x00DEAD00u);
                wstr(dat + 0x700, "TEX_A");
                wstr(dat + 0x708, "TEX_B");
            }
        }
    }
}

static int test_mlb(void)
{
    ML_WORK ml;
    unsigned char before[0x2000];
    build_mlb_file(mlb_file);
    memcpy(before, mlb_file, sizeof(before));

    if (lp64_is_on()) {
        NJS_CNK_OBJECT* obj0;
        NJS_CNK_MODEL* mdl;
        ML_WORK ml2;
        CHECK(lp64_mlb(mlb_file, &ml, LP64_SLOT_ROOM) == 1);
        CHECK(ml.flg == (0x80 | 2) && ml.obj_num == 2);
        CHECK(ml.datP == (void*)(mlb_file + MLB_HDR));
        CHECK(in_bounds(ml.objP, LP64_SLOT_ROOM) && in_bounds(ml.texP, LP64_SLOT_ROOM));

        obj0 = ml.objP;
        CHECK(obj0->evalflags == 0x11);
        CHECK(obj0->pos[0] == 1.0f && obj0->pos[2] == 3.0f);
        CHECK(obj0->ang[1] == 200 && obj0->scl[2] == 6.0f);
        CHECK(obj0->child == NULL);                       /* 0xFFFFFFFF sentinel */
        CHECK(obj0->sibling == &ml.objP[1]);              /* array-index realized */
        CHECK(ml.objP[1].model == NULL);                  /* model sentinel */
        CHECK(ml.objP[1].child == &ml.objP[0]);
        CHECK(ml.objP[1].sibling == NULL);

        mdl = (NJS_CNK_MODEL*)obj0->model;
        CHECK(in_bounds(mdl, LP64_SLOT_ROOM));
        CHECK(mdl->center.x == 1.25f && mdl->center.z == 3.25f);
        CHECK(mdl->r == 6.5f);                            /* the disc's negative sentinel cleared in the copy */
        CHECK((uintptr_t)mdl->vlist == (uintptr_t)ml.datP + 0x400);
        CHECK((uintptr_t)mdl->plist == (uintptr_t)ml.datP + 0x500);

        CHECK(ml.texP->nbTexture == 2);
        CHECK(in_bounds(ml.texP->textures, LP64_SLOT_ROOM));
        CHECK(ml.texP->textures[0].attr == 0x00110011u && ml.texP->textures[0].texaddr == 0x00C0FFEEu);
        CHECK((uintptr_t)ml.texP->textures[0].filename == (uintptr_t)ml.datP + 0x700);
        CHECK((uintptr_t)ml.texP->textures[1].filename == (uintptr_t)ml.datP + 0x708);
        CHECK(strcmp((const char*)ml.texP->textures[0].filename, "TEX_A") == 0);

        /* dedup: the same disc base realizes once */
        CHECK(lp64_mlb(mlb_file, &ml2, LP64_SLOT_ROOM) == 1);
        CHECK(ml2.objP == ml.objP && ml2.texP == ml.texP);
    } else {
        CHECK(lp64_mlb(mlb_file, &ml, LP64_SLOT_ROOM) == 0);
    }
    CHECK(memcmp(before, mlb_file, sizeof(before)) == 0);
    printf("lp64 ML tree (CNK): ok (%s)\n", lp64_is_on() ? "LP64" : "32-bit pass-through");
    return 0;
}

/* ------------------------------------------------------- BSC model fixture */

static unsigned char bsc_file[0x2000];

static void build_bsc_file(void)
{
    memset(bsc_file, 0xCE, 0x2000);
    bsc_file[3] = 2;                  /* status without 0x80: basic models */
    w16(bsc_file + 4, MLB_HDR);
    w16(bsc_file + 6, 1);
    w32(bsc_file + 8, 0xFFFFFFFFu);   /* no texlist */
    w32(bsc_file + 12, MLB_OBJ);
    {
        unsigned char* dat = bsc_file + MLB_HDR;
        unsigned char* o = dat + MLB_OBJ;
        w32(o + 4, MLB_MDL);          /* the NJS_MODEL (dat-relative) */
        w32(o + 44, 0xFFFFFFFFu);     /* child sentinel */
        w32(o + 48, 0xFFFFFFFFu);    /* sibling sentinel */
        {
            unsigned char* m = dat + MLB_MDL;
            w32(m + 0, 0x40);                          /* points */
            w32(m + 4, 0x50);                          /* normals */
            w32(m + 8, 0x80000003u);                    /* nbPoint sentinel + 3 */
            w32(m + 12, 0x200);                         /* meshsets (dat-relative) */
            w32(m + 16, 0xFFFFFFFFu);                   /* mats: sentinel kept raw */
            w16(m + 20, 1);                             /* nbMeshset */
            w16(m + 22, 4);                             /* nbMat */
            wf(m + 24, 0.5f); wf(m + 28, 1.5f); wf(m + 32, 2.5f);   /* center */
            wf(m + 36, 9.75f);                          /* r */
            /* one NJS_MESHSET (disc stride 24) */
            {
                unsigned char* s = dat + 0x200;
                w16(s + 0, 0x0123); w16(s + 2, 7);      /* type_matId, nbMesh */
                w32(s + 4, 0x300);                      /* meshes */
                w32(s + 8, 0xFFFFFFFFu);                /* attrs -> NULL */
                w32(s + 12, 0x310);                     /* normals */
                w32(s + 16, 0xFFFFFFFFu);                /* vertcolor -> NULL */
                w32(s + 20, 0xFFFFFFFFu);                /* vertuv -> NULL */
            }
        }
    }
}

static int test_bsc(void)
{
    ML_WORK ml;
    NJS_MODEL* mdl;
    unsigned char before[0x2000];
    build_bsc_file();
    memcpy(before, bsc_file, sizeof(before));

    if (lp64_is_on()) {
        CHECK(lp64_mlb(bsc_file, &ml, LP64_SLOT_SESSION) == 1);
        CHECK(ml.obj_num == 1 && ml.texP == NULL);
        mdl = (NJS_MODEL*)ml.objP[0].model;
        CHECK(in_bounds(mdl, LP64_SLOT_SESSION));
        CHECK(mdl->nbPoint == 3);                                   /* sentinel cleared in the copy */
        CHECK((uintptr_t)mdl->points == (uintptr_t)ml.datP + 0x40);
        CHECK((uintptr_t)mdl->normals == (uintptr_t)ml.datP + 0x50);
        CHECK(mdl->nbMeshset == 1 && mdl->nbMat == 4);
        CHECK(mdl->center.x == 0.5f && mdl->r == 9.75f);
        CHECK((uintptr_t)mdl->mats == 0xFFFFFFFFu);                /* raw sentinel kept, like the decomp */
        CHECK(in_bounds(mdl->meshsets, LP64_SLOT_SESSION));
        CHECK(mdl->meshsets[0].type_matId == 0x0123 && mdl->meshsets[0].nbMesh == 7);
        CHECK((uintptr_t)mdl->meshsets[0].meshes == (uintptr_t)ml.datP + 0x300);
        CHECK(mdl->meshsets[0].attrs == NULL);
        CHECK((uintptr_t)mdl->meshsets[0].normals == (uintptr_t)ml.datP + 0x310);
        CHECK(mdl->meshsets[0].vertcolor == NULL && mdl->meshsets[0].vertuv == NULL);
    } else {
        CHECK(lp64_mlb(bsc_file, &ml, LP64_SLOT_SESSION) == 0);
    }
    CHECK(memcmp(before, bsc_file, sizeof(before)) == 0);
    printf("lp64 ML tree (BSC): ok (%s)\n", lp64_is_on() ? "LP64" : "32-bit pass-through");
    return 0;
}

/* ----------------------------------------------------------- MN motion fixture */

static unsigned char mnb_file[0x1000];

static void build_mnb_file(void)
{
    memset(mnb_file, 0xCF, 0x1000);
    mnb_file[3] = 3;                   /* flg */
    w16(mnb_file + 4, MLB_HDR);       /* header size */
    w16(mnb_file + 6, 2);              /* obj_num (MDATA2 entries) */
    w32(mnb_file + 8, 0x100);          /* the NJS_MOTION, dat_top-relative */
    w32(mnb_file + 12, 0x400);         /* atr, dat_top-relative */
    {
        unsigned char* top = mnb_file + MLB_HDR;
        unsigned char* mtn = top + 0x100;   /* NJS_MOTION (disc 12) */
        w32(mtn + 0, 0x200);                /* mdata -> the MDATA2 array */
        w32(mtn + 4, 120);                  /* nbFrame */
        w16(mtn + 8, 0x0002); w16(mtn + 10, 0x0008);   /* type, inp_fn */
        {
            unsigned char* md2 = top + 0x200;
            w32(md2 + 0, 0x500); w32(md2 + 4, 0xFFFFFFFFu);   /* p0 valid, p1 NULL */
            w32(md2 + 8, 0xFFFFFFFFu); w32(md2 + 12, 0x510);  /* entry 1: p0 NULL, p1 valid */
        }
    }
}

static int test_mnb(void)
{
    MN_WORK mn;
    unsigned char before[0x1000];
    build_mnb_file();
    memcpy(before, mnb_file, sizeof(before));

    if (lp64_is_on()) {
        CHECK(lp64_mnb(mnb_file, &mn, LP64_SLOT_SESSION) == 1);
        CHECK(mn.flg == 3 && mn.obj_num == 2);
        CHECK(mn.frm_num == 120);
        CHECK(mn.datP == (void*)mnb_file);
        CHECK(in_bounds(mn.md2P, LP64_SLOT_SESSION));
        CHECK((uintptr_t)mn.md2P[0].p[0] == (uintptr_t)mnb_file + MLB_HDR + 0x500);
        CHECK(mn.md2P[0].p[1] == NULL);
        CHECK(mn.md2P[1].p[0] == NULL);
        CHECK((uintptr_t)mn.md2P[1].p[1] == (uintptr_t)mnb_file + MLB_HDR + 0x510);
        CHECK((uintptr_t)mn.atrP == (uintptr_t)mnb_file + MLB_HDR + 0x400);
        CHECK(lp64_mnb(mnb_file, &mn, LP64_SLOT_SESSION) == 1);   /* dedup hit */
    } else {
        CHECK(lp64_mnb(mnb_file, &mn, LP64_SLOT_SESSION) == 0);
    }
    CHECK(memcmp(before, mnb_file, sizeof(before)) == 0);
    printf("lp64 MN motion: ok (%s)\n", lp64_is_on() ? "LP64" : "32-bit pass-through");
    return 0;
}

/* -------------------------------------------------------- arena reset behavior */

static int test_arena_reset(void)
{
    if (lp64_is_on()) {
        ML_WORK a, b;
        build_mlb_file(mlb_file);
        lp64_reset(LP64_SLOT_ROOM);   /* baseline the arena: both unpacks start from its base */
        CHECK(lp64_mlb(mlb_file, &a, LP64_SLOT_ROOM) == 1);
        lp64_reset(LP64_SLOT_ROOM);
        CHECK(lp64_mlb(mlb_file, &b, LP64_SLOT_ROOM) == 1);
        /* the room arena restarts at its base: the re-unpack lands at the same address */
        CHECK(b.objP == a.objP && b.texP == a.texP);
        /* and a different disc base unpacks to a different tree */
        {
            static unsigned char other[0x2000];
            ML_WORK c;
            build_mlb_file(other);
            other[0x300] ^= 0xFF;   /* not the same disc base */
            CHECK(lp64_mlb(other, &c, LP64_SLOT_ROOM) == 1);
            CHECK(c.objP != b.objP);
        }
    } else {
        lp64_reset(LP64_SLOT_ROOM);   /* pass-through: a no-op */
    }
    printf("lp64 arena reset + dedup: ok (%s)\n", lp64_is_on() ? "LP64" : "32-bit pass-through");
    return 0;
}

int main(void)
{
    CHECK(!test_rom_work());
    CHECK(!test_mlb());
    CHECK(!test_bsc());
    CHECK(!test_mnb());
    CHECK(!test_arena_reset());
    printf("lp64 realize: ok\n");
    return 0;
}
