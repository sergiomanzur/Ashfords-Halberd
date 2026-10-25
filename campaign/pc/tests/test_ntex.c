/* Native texture queue (pc/src/gs/textures.c): deferred image uploads on synthetic definitions,
   GS memory stubbed out. Which uploads reach GS memory, in which order: superseded ones never,
   the rest only when their pages are accessed or their definition is forgotten. */
#include "../src/gs/gs.h"
#include "../src/gs/textures.h"
#include "../src/render/gl_loader.h"
#include "../src/gs/snapshot_codec.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

/* the stubs textures.c needs; uploads are logged as "image byte 0" in order */
GsState gs;
int gs_replaying, gs_flush_blocked;
uint32_t gs_reg_serial;
uint8_t gs_mem[GS_MEM_SIZE];
uint8_t* gs_memp = gs_mem;
PFN_glGenTextures gl_GenTextures;
PFN_glBindTexture gl_BindTexture;
PFN_glPixelStorei gl_PixelStorei;
PFN_glTexImage2D gl_TexImage2D;
static void GLAPIENTRY unexpected_texture_delete(GLsizei n,const GLuint* textures) { (void)n;(void)textures;abort(); }
PFN_glDeleteTextures gl_DeleteTextures=unexpected_texture_delete;
/* This CPU-only queue fixture has no captured GPU images. Fail immediately if
   a future test accidentally requires a GPU; the real GL fixture covers them. */
int gs_snapshot_write_texture(RenderWriter* w,unsigned int texture) { (void)w;(void)texture;abort(); }
int gs_snapshot_read_texture(RenderReader* r,RenderStagedImage* out,int stage) { (void)r;(void)out;(void)stage;abort(); }
void gs_snapshot_drop_texture(RenderStagedImage* image) { if (image->tex) abort(); }
static char wrote[64];
static int nwrote;
void gs_exec_reg(int reg, uint64_t v) { (void)reg; (void)v; }
void gs_exec_upload(const uint8_t* src, uint32_t bytes) { (void)bytes; if (nwrote < 63) wrote[nwrote++] = (char)src[0]; wrote[nwrote] = 0; }
void gs_flush(void) {}
void gs_tex_delete(unsigned int t) { (void)t; }
void gs_mem_scratch_begin(void) {}
void gs_mem_scratch_end(void) {}
void gs_decode_texture(uint64_t tex0, uint64_t texa, uint32_t w, uint32_t h, uint32_t* out) { (void)tex0; (void)texa; (void)w; (void)h; (void)out; }
void gs_get_palette(uint64_t tex0, uint64_t texa, uint32_t* out, int n) { (void)tex0; (void)texa; (void)out; (void)n; }
uint32_t gs_pixel_addr(int psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) { (void)psm; (void)bw; (void)x; (void)y; return bp * 256; }
int hdpack_active(void) { return 0; }
void hdpack_note(unsigned int t, uint64_t tex0, uint64_t texa, int flags) { (void)t; (void)tex0; (void)texa; (void)flags; }
void hdpack_forget(unsigned int t) { (void)t; }

/* definition `id` named by the letter c: an image of `pages` pages at block 0 */
static void define(uint32_t id, char c, uint32_t pages)
{
    uint8_t img[16];
    memset(img, c, sizeof(img));
    ntex_define_begin(id, 0, 0x3FFF, pages, 0);
    ntex_define_reg(0x50, 0ull << 32);
    ntex_define_image(img, sizeof(img));
    ntex_define_end();
}

/* definition `id` uploading a w x h image of format psm at the slot from (0, 0), buffer width bw */
static void define_rect(uint32_t id, char c, uint32_t pages, uint32_t psm, uint32_t bw, uint32_t w, uint32_t h)
{
    static uint8_t img[64 * 128 * 4];
    memset(img, c, sizeof(img));
    ntex_define_begin(id, 0, 0x3FFF, pages, 0);
    ntex_define_reg(0x50, (uint64_t)psm << 56 | (uint64_t)bw << 48);
    ntex_define_reg(0x51, 0);
    ntex_define_reg(0x52, (uint64_t)h << 32 | w);
    ntex_define_reg(0x53, 0);
    ntex_define_image(img, w * h * (psm == 0x13 ? 1 : 4));
    ntex_define_end();
}

static const char* took(void) { static char s[64]; strcpy(s, wrote); nwrote = 0; wrote[0] = 0; return s; }

int main(void)
{
    const uint32_t X = 380 * 32, Y = 382 * 32;   /* two slots: pages 380.. and 382.. */
    define_rect(6, 'F', 5, 0x00, 1, 64, 128);   /* PSMCT32 64 x 128: pages 0..3 entirely (5 pages from the slot as texture_glue.c counts) */
    define_rect(7, 'H', 5, 0x1b, 1, 64, 128);   /* the same as PSMT8H: only the high bytes */
    define_rect(8, 'G', 5, 0x00, 2, 64, 128);   /* half as wide as its buffer */
    define(1, 'A', 3); define(2, 'B', 3); define(3, 'C', 3); define(4, 'S', 1); define(5, 'D', 3);

    /* the game's texture slot, switched thousands of times next to an overlapping one: nothing
       written (the queue never fills up) */
    for (int i = 0; i < 5000; i++) { ntex_bind(i & 1 ? 1 : 3, X, 0x3FFF); ntex_bind(2, Y, 0x3FFF); }
    CHECK(!strcmp(took(), ""));
    ntex_before_access(381, 381);   /* a read: only the last one there */
    CHECK(!strcmp(took(), "A"));
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "B"));

    /* superseded although an upload queued in between overlaps it */
    ntex_bind(1, X, 0x3FFF);        /* A: 380..382 */
    ntex_bind(2, Y, 0x3FFF);        /* B: 382..384 */
    ntex_bind(3, X, 0x3FFF);        /* C: 380..382, covers A */
    CHECK(!strcmp(took(), ""));
    ntex_before_access(384, 384);   /* B's pages outside C: B alone */
    CHECK(!strcmp(took(), "B"));
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "C"));

    /* a larger one is not superseded by a smaller one; both written in order when read */
    ntex_bind(1, X, 0x3FFF);        /* A: 380..382 */
    ntex_bind(4, X, 0x3FFF);        /* S: 380 */
    ntex_before_access(380, 380);
    CHECK(!strcmp(took(), "AS"));

    /* written before something overlapping is queued behind it and then read */
    ntex_bind(1, X, 0x3FFF);
    ntex_bind(2, Y, 0x3FFF);
    ntex_before_access(383, 383);   /* B, and A first under it */
    CHECK(!strcmp(took(), "AB"));

    /* forgetting a definition: nothing if it was superseded, its own upload if still queued */
    ntex_bind(1, X, 0x3FFF);
    ntex_bind(3, X, 0x3FFF);
    ntex_forget(1);
    CHECK(!strcmp(took(), ""));
    ntex_bind(2, Y, 0x3FFF);
    ntex_forget(2);                 /* B overlaps the queued C: C first */
    CHECK(!strcmp(took(), "CB"));
    ntex_bind(5, X, 0x3FFF);
    ntex_bind(3, X, 0x3FFF);        /* D superseded, C queued */
    ntex_forget(5);
    CHECK(!strcmp(took(), ""));
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "C"));

    /* a smaller upload elsewhere within the pages a later one overwrites entirely */
    ntex_bind(4, 381 * 32 + 8, 0x3FFF);   /* S: page 381 */
    ntex_bind(3, 382 * 32, 0x3FFF);       /* C: 382..384, reaches past F */
    ntex_bind(6, X, 0x3FFF);              /* F: 380..383 written */
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "CF"));
    ntex_bind(4, 381 * 32 + 8, 0x3FFF);
    ntex_bind(6, X + 8, 0x3FFF);          /* F not page-aligned */
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "SF"));
    ntex_bind(4, 381 * 32 + 8, 0x3FFF);
    ntex_bind(7, X, 0x3FFF);              /* H keeps the low bytes */
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "SH"));
    ntex_bind(4, 381 * 32 + 8, 0x3FFF);
    ntex_bind(8, X, 0x3FFF);              /* G leaves half of each page */
    ntex_before_access(0, 511);
    CHECK(!strcmp(took(), "SG"));

    /* replays write nothing */
    gs_replaying = 1;
    ntex_bind(3, X, 0x3FFF);
    ntex_before_access(0, 511);
    gs_replaying = 0;
    CHECK(!strcmp(took(), ""));
    /* Native definitions retain their generation IDs and uploads without GL. */
    RenderWriter snapshot={0};ntex_snapshot_write(&snapshot);CHECK(!snapshot.bad);
    RenderReader reader={snapshot.data,snapshot.size,0,0};
    void* staged=ntex_snapshot_read(&reader,1);CHECK(staged && rs_finish(&reader));
    ntex_snapshot_commit(staged);free(snapshot.data);
    ntex_bind(3,X,0x3FFF);ntex_before_access(0,511);CHECK(!strcmp(took(),"C"));
    printf("ntex: ok\n");
    return 0;
}
