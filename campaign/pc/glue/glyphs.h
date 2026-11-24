/* Vector controller & keyboard button glyphs and status marks for native menus (glyphs.c). */
#ifndef RECVX_GLYPHS_H
#define RECVX_GLYPHS_H

enum { UG_OK, UG_BACK, UG_CAT, UG_CHANGE, UG_START };

void ui_glyph_device(unsigned int press, int kb_used);
int  ui_glyph_using_pad(void);
const char* ui_glyph_str(int which, char* buf, int n);
float ui_glyph_hint_w(const char* glyph, const char* word);
float ui_glyph_hint(float x, float y, const char* glyph, const char* word);
void ui_glyph_bar(float x, float y, const int* actions, const char* const* words, int count);
void ui_glyph_mark(float x, float y, int kind);   /* 1: restart [R], 2: next-room turning arrow */

#endif
