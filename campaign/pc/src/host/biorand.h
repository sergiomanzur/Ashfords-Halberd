/* Offline BioRand pack loader (https://github.com/biorand/classic, MIT): reads a locally
   generated BioRand pack in <mods>/biorand/ (RM_*.RDX room files, SYSTEM.AFS/10 & /11 player
   models, log_pl0.txt spoiler log). Strictly offline: no network downloading or remote sync. */
#ifndef RECVX_BIORAND_H
#define RECVX_BIORAND_H
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

/* BioRand's seed number from its seed string "R840-0KR3-T4HH..." (20-bit seed, low 5 bits first); -1 none */
static inline int pc_biorand_seed(const char* s)
{
    static const char abc[] = "0123456789ABCEFGHJKLMNPQRTUVWXYZ";
    int v = 0, i;
    for (i = 0; i < 10; i++)
        if (!s[i]) return -1;
    if (s[0] != 'R' || s[4] != '-' || s[9] != '-') return -1;
    for (i = 0; i < 4; i++) {
        const char* p = memchr(abc, s[5 + i] >= 'a' && s[5 + i] <= 'z' ? s[5 + i] - 32 : s[5 + i], 32);
        if (!p) return -1;
        v |= (int)(p - abc) << (5 * i);
    }
    return v;
}

void biorand_scan(void);               /* inspect <mods>/biorand for a locally generated pack */
int  biorand_pack(void);               /* 1 if a generated pack is present in <mods>/biorand */
int  biorand_seed(void);               /* seed number (0..1048575), -1 unknown */
unsigned int biorand_id(void);         /* FNV-1a hash of log_pl0.txt + room files; 0 none */
const char* biorand_seed_text(void);   /* "seed 123456", "" unknown */
const char* biorand_seed_string(void); /* 68-char decodable seed string, "" unknown */
int  biorand_active(void);             /* 1 while the current game uses the BioRand pack */
void biorand_game(int on);             /* set current game's BioRand state (0 at title, 1 on BioRand play) */
const char* biorand_locked(void);      /* why g_config.biorand cannot change now, or NULL */
int  biorand_rifle_stack(void);        /* rifle ammo stacks: g_config.rifle_stack || biorand_active() */
void* biorand_room0(int* size);        /* expanded <mods>/biorand/RM_0000.RDX (malloc'd, free it) or NULL */

enum { SPOILER_NONE, SPOILER_OK, SPOILER_MISSING, SPOILER_BROKEN };
struct Spoiler;
const struct Spoiler* biorand_spoiler(int* state);

void biorand_frame(void);              /* game thread, every frame (biorand_glue.c) */
void pc_biorand_stamp(void);           /* stamp current BioRand state into sys->itm[288..289] */
void pc_biorand_save_apply(const void* save);   /* activate/deactivate BioRand from loaded save words */
void spoiler_key(int k);               /* main thread -> game thread: 1 open/close, 2 Esc, 3..6 PgUp/PgDn/Home/End */

#ifdef __cplusplus
}
#endif
#endif
