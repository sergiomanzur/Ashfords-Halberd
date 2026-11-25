/* Restore bridged game data macros after static table initializers. */
#ifdef ASHFORD_ANDROID_GAME_BRIDGE
#undef g_appear_chr
#undef g_cheat
#undef g_enemy_opts
#undef g_lang
#undef g_lang_pictures
#undef g_lang_voices
#undef g_subtitles
#undef g_tps
#define g_appear_chr     (*ashford_ptr_g_appear_chr)
#define g_cheat          (*ashford_ptr_g_cheat)
#define g_enemy_opts     (*ashford_ptr_g_enemy_opts)
#define g_lang           (*ashford_ptr_g_lang)
#define g_lang_pictures  (*ashford_ptr_g_lang_pictures)
#define g_lang_voices    (*ashford_ptr_g_lang_voices)
#define g_subtitles      (*ashford_ptr_g_subtitles)
#define g_tps            (*ashford_ptr_g_tps)
#endif
