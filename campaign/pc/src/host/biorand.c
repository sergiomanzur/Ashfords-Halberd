/* Offline BioRand pack scanner and spoiler log cache (biorand.h). Strictly local filesystem only. */
#include "../ps2_platform.h"
#include "biorand.h"
#include "biorand_opts.h"
#include "spoiler.h"
#include "fileio.h"
#include "../../include/pc_parse.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pc_in_game(void);

static struct {
    int pack, seed;
    unsigned int id;
    char text[32];
    char str[BIORAND_SEED_LEN + 1];
    int game;
} br = { .seed = -1 };

static const char* mods_dir(void) { return SDL_getenv("RECVX_MODS") ? SDL_getenv("RECVX_MODS") : "mods"; }

static unsigned int fnv(unsigned int h, const void* p, size_t n)
{
    const unsigned char* b = (const unsigned char*)p;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}

void biorand_scan(void)
{
    char dir[1100], path[1200], line[256];
    char** list;
    int n = 0, i, rooms = 0, seed = -1, log = 0;
    unsigned int id = 2166136261u, sum = 0;
    FILE* f;
    snprintf(dir, sizeof(dir), "%s/biorand", mods_dir());
    if ((list = SDL_GlobDirectory(dir, "RM_*.RDX", SDL_GLOB_CASEINSENSITIVE, &n))) {
        for (i = 0; i < n; i++) {
            SDL_PathInfo pi;
            snprintf(path, sizeof(path), "%s/%s", dir, list[i]);
            if (!SDL_GetPathInfo(path, &pi)) continue;
            sum += fnv(fnv(2166136261u, list[i], strlen(list[i])), &pi.size, sizeof(pi.size));
            rooms++;
        }
        SDL_free(list);
    }
    br.str[0] = 0;
    snprintf(path, sizeof(path), "%s/log_pl0.txt", dir);
    if ((f = pc_fopen(path, "rb"))) {
        size_t k;
        unsigned char buf[4096];
        log = 1;
        while (fgets(line, sizeof(line), f)) {
            const char* l = line + strspn(line, " \t");
            if (!strncmp(l, "Seed: ", 6)) {
                BioOpts o;
                seed = pc_biorand_seed(l + 6);
                l += 6;
                snprintf(br.str, sizeof(br.str), "%.*s", (int)strcspn(l, " \t\r\n"), l);
                if (strlen(br.str) != BIORAND_SEED_LEN || biorand_seed_decode(br.str, &o)) br.str[0] = 0;
                break;
            }
        }
        rewind(f);
        while ((k = fread(buf, 1, sizeof(buf), f)) > 0) id = fnv(id, buf, k);
        fclose(f);
    }
    id = fnv(id, &sum, sizeof(sum));
    br.seed = rooms || log ? seed : -1;
    br.id = rooms || log ? (id ? id : 1) : 0;
    if (br.seed >= 0) snprintf(br.text, sizeof(br.text), "seed %d", br.seed);
    else br.text[0] = 0;
    br.pack = rooms || log;
    if (br.pack)
        host_log("biorand: pack in %s: %d room files, %s (id %08x); toggle %s\n", dir, rooms,
                 br.seed >= 0 ? br.text : "seed unknown", br.id, g_config.biorand ? "on" : "off");
}

int biorand_pack(void) { return br.pack; }
int biorand_seed(void) { return br.seed; }
unsigned int biorand_id(void) { return br.id; }
const char* biorand_seed_text(void) { return br.text; }
const char* biorand_seed_string(void) { return br.str; }

int biorand_active(void) { return br.pack && br.game; }

void biorand_game(int on)
{
    int want = br.pack && on;
    if (br.game != want) host_log("biorand: this game %s BioRand\n", want ? "plays" : "does not play");
    br.game = want;
}

const char* biorand_locked(void)
{
    if (!br.pack) return "Place a BioRand pack in mods/biorand first.";
    if (pc_in_game()) return "Change at the title screen.";
    return NULL;
}

/* BioRand's PS2 build patches rifle stacking in; its amounts count on it */
int biorand_rifle_stack(void) { return g_config.rifle_stack || biorand_active(); }

void* biorand_room0(int* size)
{
    char path[1200];
    unsigned char *in = NULL, *d = NULL;
    long n, cap = 4 << 20, m = -1;
    FILE* f;
    snprintf(path, sizeof(path), "%s/biorand/RM_0000.RDX", mods_dir());
    if (!(f = pc_fopen(path, "rb"))) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0 && n < (32 << 20) && (in = malloc((size_t)n)) && (d = malloc((size_t)cap)) &&
        fread(in, 1, (size_t)n, f) == (size_t)n)
        m = pc_prs_expand(in, n, &d, &cap);
    fclose(f);
    free(in);
    if (m < 0x80) { free(d); return NULL; }
    *size = (int)m;
    return d;
}

/* ---------------------------------------------------------------- cached spoiler log */
static struct { Spoiler* sp; unsigned int id; long long size, mtime; int state; } sl;

static void spoiler_free(void) { free(sl.sp); sl.sp = NULL; sl.id = 0; sl.state = SPOILER_NONE; }

const Spoiler* biorand_spoiler(int* state)
{
    static int registered;
    char path[1200];
    SDL_PathInfo pi;
    char* text = NULL;
    FILE* f;
    long n = -1;
    snprintf(path, sizeof(path), "%s/biorand/log_pl0.txt", mods_dir());
    if (!SDL_GetPathInfo(path, &pi)) pi.size = (Uint64)-1, pi.modify_time = 0;
    if (sl.state != SPOILER_NONE && sl.id == biorand_id() && sl.size == (long long)pi.size && sl.mtime == (long long)pi.modify_time) {
        *state = sl.state;
        return sl.sp;
    }
    if (!registered++) atexit(spoiler_free);
    spoiler_free();
    sl.id = biorand_id(); sl.size = (long long)pi.size; sl.mtime = (long long)pi.modify_time;
    sl.state = SPOILER_MISSING;
    if ((f = pc_fopen(path, "rb"))) {
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        sl.state = SPOILER_BROKEN;
        if (n >= 0 && n <= (4 << 20) && (text = malloc((size_t)n + 1)) && fread(text, 1, (size_t)n, f) == (size_t)n &&
            (sl.sp = malloc(sizeof(Spoiler))) && !spoiler_parse(text, (size_t)n, sl.sp))
            sl.state = SPOILER_OK;
        fclose(f);
        free(text);
        if (sl.state != SPOILER_OK) { free(sl.sp); sl.sp = NULL; }
    }
    host_log("biorand: spoiler log %s: %s", path,
             sl.state == SPOILER_OK ? "read" : sl.state == SPOILER_MISSING ? "missing" : "not a BioRand log");
    if (sl.sp) host_log(" (%d spots, %d rooms, %d segments%s)", sl.sp->nitem, sl.sp->nroom, sl.sp->nseg, sl.sp->truncated ? ", cut" : "");
    host_log("\n");
    *state = sl.state;
    return sl.sp;
}
