/* Enemy options (docs/ENEMIES.md "Enemy options"): extra enemies, rearranged placements,
   randomizer, custom per-type rates, and cutscene cast options (offline single-player). */
#ifndef RECVX_ENEMIES_H
#define RECVX_ENEMIES_H
#ifdef __cplusplus
extern "C" {
#endif

#define ENEMY_EXTRA_MAX 8       /* "Extra enemies": up to this many per room, each with a dead flag of its own (enemy_xdead_word) */
#define ENEMY_XDEAD_WORDS 41    /* the extras' dead flags: save-region words (sys->itm 302..319, 360..382), a bit per room and extra */
#define ENEMY_XDEAD_BASE 1024   /* SpawnNote.dead of an extra enemy: this + its dead flag's bit */
#define ENEMY_TYPES_MAX 8       /* "Types per room": 1..8, 0 no limit */
#define ENEMY_RATE_MAX 10       /* a kind's rate under "Custom": 0 (Off) .. 10 */
#define ENEMY_RATE_N 32         /* EnemyOpts.rate: the base kinds (enemy_rate_*), then the enemy variants by variant_at index */

typedef struct EnemyOpts {
    int extra;                  /* 0 off, 1..ENEMY_EXTRA_MAX: up to that many extra enemies per room */
    int place;                  /* 1: rearranged placements */
    int rand;                   /* 1: enemy randomizer */
    int seed;                   /* placements and randomizer (0..2^31-1) */
    int variety;                /* randomizer: 0 Mild (two common kinds per room), 1 Wild (every kind that fits; default),
                                   2 Custom (each kind by its rate[]) */
    int events;                 /* randomizer: 1 also event enemies (default) */
    int cast;                   /* cutscene cast randomizer (cast.h): 1 the story characters drawn as other models in cutscenes */
    int cast_all;               /* its characters: 0 from the cutscene models (faces move), 1 from all models, 2 "Custom" */
    int cast_assign;            /* 0 fixed per character (the seed), 1 random per cutscene (seed and room file) */
    int cast_imports;           /* 1: include imported Outbreak models in cutscene cast pools */
    int voices;                 /* 1: "Randomize voice lines": a role shown as another story character's model says that one's lines */
    int types;                  /* randomizer: at most this many different kinds in a room (0: no limit) */
    unsigned char rate[ENEMY_RATE_N];   /* "Custom": each kind's weight, 0..ENEMY_RATE_MAX (recvx.ini enemy_rates); picking Wild or
                                           Mild presets them (enemy_rate_preset) */
    int cast_enemy;             /* the cutscene cast's "Enemy models" (cast.h CAST_ENEMY_*): what its creatures and scene zombies become */
    unsigned char cast_enemy_off[4];    /* ... its "Custom" list's unchecked entries (cast.h CAST_ENEMY_BYTES; recvx.ini enemy_custom_off) */
} EnemyOpts;
extern EnemyOpts g_enemy_opts;  /* our settings (F1, recvx.ini) */

/* "Cutscene monsters" without the cutscene cast: a flag in the last bit of cast_enemy_off, past its list's entries.
   Set, the creatures and scene zombies are cast by cast_enemy (All enemies or Custom) while cast is off. */
static inline int enemy_cast_alone(const EnemyOpts* o) { return (o->cast_enemy_off[3] & 0x80) != 0; }
static inline void enemy_cast_alone_set(EnemyOpts* o, int on) { o->cast_enemy_off[3] = (unsigned char)((o->cast_enemy_off[3] & 0x7F) | (on ? 0x80 : 0)); }
static inline int enemy_cast_creatures(const EnemyOpts* o) { return o->cast || (enemy_cast_alone(o) && (o->cast_enemy == 1 || o->cast_enemy == 2)); }

/* "Custom"'s list (enemyopt_glue.c): every kind the randomizer can place, the base kinds then the enemy variants with a
   randomizer weight; i is the kind's index into EnemyOpts.rate */
int  enemy_rate_count(void);
int  enemy_rate_at(int k);                      /* the list's k-th kind: its rate index */
const char* enemy_rate_name(int i);
void enemy_rate_preset(unsigned char* rate, int variety);   /* Wild's (1) or Mild's (0) weights */
void enemy_rates_ini(const char* text);         /* config.c: the recvx.ini line, applied by enemy_rates_load */
void enemy_rates_load(void);                    /* at start, once the variants are known (host.c) */
const char* enemy_rates_text(void);             /* the recvx.ini line ("1:5 4:4 ... v1:1": type ids, v + variant ids) */

/* an enemy spawned dynamically (spawner, extra enemies, randomizer): where its data is and where it stands */
typedef struct SpawnNote {
    unsigned short room;        /* source room file RM_<room>.RDX and its record */
    unsigned char rec, id, mdlver, type;
    signed char flr;
    unsigned char pad;
    float px, py, pz;
    short ay;
    unsigned short variant;     /* an enemy variant's id (pc/src/host/variants.h; 0: the type itself) */
    unsigned short dead;        /* an extra enemy's dead flag (ENEMY_XDEAD_BASE + its bit; 0: none) */
} SpawnNote;

/* spawn_at's `mode` (pc/glue/spawn_glue.c) */
#define SPAWN_LOADING 1             /* from the room load (the randomizer): no readiness checks */
#define SPAWN_DICTATED 2            /* no texture reserve, only what the upload needs */

/* enemy slots the spawner and the randomizer leave free for what enemies spawn at run time (moth
   eggs, damage dummies; bhSetEnemy has no NULL check in their callers) */
#define SPAWN_SLOTS_FREE 6
int  spawn_slots_needed(int id);                /* a type and the enemies its Init spawns */

/* game thread */
const EnemyOpts* enemy_opts(void);              /* in effect: g_enemy_opts */
int  room_file_now(void);                       /* the room file we are in: RM_<stg * 1000 + rom * 10 + case> */
unsigned int* enemy_xdead_word(int i);          /* word i (< ENEMY_XDEAD_WORDS) of the extras' dead flags */

#ifdef __cplusplus
}
#endif
#endif
