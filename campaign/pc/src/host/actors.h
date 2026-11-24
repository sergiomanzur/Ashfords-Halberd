/* Enemy and NPC models a player can appear as ("You appear as", docs/ACTORS.md "You appear as").
   The catalogue names where each model is found on the disc (room file and enemy record); the
   game thread loads it from there (pc/glue/actor_glue.c). No disc data lives here. */
#ifndef RECVX_ACTORS_H
#define RECVX_ACTORS_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ActorModel {
    const char* name;
    unsigned short room;       /* RM_<room>.RDX, e.g. 1140 */
    unsigned char rec;         /* enemy record of the room that carries the model */
    unsigned char id, mdlver;  /* expected record type and model version (checked at load) */
    unsigned char anim;        /* driven by the player's skeleton (if a humanoid node map is found at
                                  load) with the motions of: ANIM_PLAYED the character played,
                                  ANIM_MALE / ANIM_FEMALE a man's (Chris's) / a woman's (Claire's)
                                  when "gender-matched animations" is on; ANIM_IDLE (0): plays its
                                  own motion `idle`, looped */
    unsigned char idle;
} ActorModel;

enum { ANIM_IDLE, ANIM_PLAYED, ANIM_MALE, ANIM_FEMALE };

#define ACTOR_MODELS 62
extern const ActorModel actor_models[];

/* Future/offline-safe import table slot (0 entries unless populated by tests or a later import module). */
#define ACTOR_IMPORT_MAX 2048
extern ActorModel actor_imports[ACTOR_IMPORT_MAX];
extern const char* actor_import_file[ACTOR_IMPORT_MAX];
extern unsigned char actor_import_group[ACTOR_IMPORT_MAX];
extern int actor_import_n;
const ActorModel* actor_model(int idx);   /* catalogue entry or import idx, NULL if none */

extern int g_appear_chr;
void appear_frame(void);
int  appear_look_of(int pid);             /* effective resolved chr for character pid (0..3) */

/* "Use gender-matched animations" (ini appear_gender_anim, default 1) */
extern int g_appear_anim;
/* "Talking faces in cutscenes" (ini appear_cutscene_face, default 0, experimental) */
extern int g_appear_face;
/* "Swapped model in cutscenes" (ini appear_in_cutscenes, default 1) */
extern int g_appear_cutscene;

int appear_motion_chr(int anim, int played);
int appear_part_anim(int file);
int appear_voice(int chr);
int appear_grab_motion(int id, int mtn, int from, int to);

/* "You appear as" (g_appear_chr): 0 the character played, 1..5 the player files, then these */
#define COOP_CHR_ACTOR 6
#define COOP_CHR_MAX (COOP_CHR_ACTOR + ACTOR_MODELS - 1)
#define COOP_CHR_IMPORT (COOP_CHR_MAX + 1)
/* "Current Character (Cutscene)": the played character's cutscene model. */
#define COOP_CHR_CINE 4095
int appear_played(int chr, int ply_id);
/* "Character models" (ini appear_per_char): 0 Same for both, 1 Per character (Claire's / Chris's) */
extern int g_appear_per;
extern int g_appear_own[2];
int appear_choice(int one, int c);

int appear_count(void);
int appear_at(int i);
int appear_pos(int chr);
int appear_canon(int chr);
int appear_step(int chr, int dir);
const char* appear_name(int chr);
const char* appear_group(int chr);

/* game thread (pc/glue/actor_glue.c). who: display slot 0 our player, 1 unused offline,
   ACTOR_DOUBLE the cutscene double of the character played, ACTOR_VARIANT + k enemy variants,
   ACTOR_CAST + k cutscene cast slots. */
#define ACTOR_DOUBLE 2
#define ACTOR_VARIANT 3
#define ACTOR_CAST 5
#define ACTOR_SLOTS (ACTOR_CAST + 6)
#define ACTOR_PART 10000
int  actor_set(int who, int idx);
int  actor_shown(int who);
int  actor_shown_model(int who);
int  actor_rigged(int who);
int  actor_talks(int who);
struct BH_PWORK;
struct O_WRK;
void actor_weapon_hands(int who, struct O_WRK* wpn, int n);
void actor_pose(int who, struct BH_PWORK* src);
void actor_draw(int who);
struct BH_PWORK* actor_work(int who);
int  actor_hand_free(int who, int node);
int  actor_head(int who, float* out);
int  actor_head_node(int who);
void actor_doubles(int idx);
int  actor_double_model(int idx, int pid, int* kept);
int  actor_double_shown(const struct BH_PWORK* ep);
void actor_double_tick(void);
int  actor_double_acting(void);
int  actor_double_kept(void);
void actor_act(int who, struct BH_PWORK* ep);
int  actor_put_as(int who, struct BH_PWORK* ep);
int  actor_own_hand(int who, int side, int* node);
int  actor_src_head(int who, int node);
void actor_cast_hands(int who);
int  actor_cast_hands_chr(int who);
void actor_source(int who, struct BH_PWORK* src, int mesh, int motion, float pitch);
int  actor_put_body(int who, struct BH_PWORK* src);
int  actor_frame(const struct BH_PWORK* p);
void actor_textures_released(void);
void actor_snapshot_before_restore(void);
void actor_snapshot_after_restore(void);

typedef struct ActorData {
    const void* rec;
    const unsigned char *mdl, *mtn;
    int mlen, nlen, ntex;
    const unsigned char* tex[16];
} ActorData;
int actor_find(const ActorModel* am, ActorData* out);
const unsigned char* actor_fx_tex(int room, int fx);
int  afs_read(int part, int file, void* buf, int cap);
void player_model_read(struct BH_PWORK* who, unsigned char* file, unsigned char* mdl, unsigned char* bmt, unsigned char* mth, unsigned char* zmt);
void player_weapon_read(struct BH_PWORK* who, struct O_WRK* objs, int wpn, unsigned char* file, unsigned char* wr, unsigned char* wl, unsigned char* wmt);
void player_hair(struct O_WRK* objs, struct BH_PWORK* who, int chr, unsigned char* exp0, unsigned char* exp3);
void actor_tex_free(int* entries, int* heap);

#ifdef __cplusplus
}
#endif
#endif
