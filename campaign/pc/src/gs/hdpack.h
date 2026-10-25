/* HD texture packs made for PCSX2 (hdpack.c, docs/HD_PACK.md). Render thread only. */
#pragma once
#include <stdint.h>

/* a texture was just decoded into GL texture `t` from TEX0/TEXA as GS memory (gs_mem) and the
   CLUT buffer hold it now: look up its PCSX2 replacement. flags: the definition's NTEX_* tags
   (textures.h), 0 for textures from GS memory */
void hdpack_note(unsigned int t, uint64_t tex0, uint64_t texa, int flags);
/* the texture to bind for `t`: its replacement (loaded on first use, in the background unless
   the frame is under a fade that lets the picture through; *ascale becomes the alpha scale that
   gives GS alpha) or t itself */
unsigned int hdpack_bind(unsigned int t, float* ascale);
/* a game frame is executed; faded: the screen fade covers it partly (stream.c Frame.fade) */
void hdpack_frame(int faded);
/* GL texture `t` is being deleted (its name may be reused) */
void hdpack_forget(unsigned int t);
/* a pack is loaded (the GS memory pages of a texture must be current before hdpack_note) */
int hdpack_active(void);
