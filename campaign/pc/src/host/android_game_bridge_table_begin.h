/* Temporarily map bridged game data macros to compile-time hidden sentinels
 * inside static table initializers (config.c keys[] and settings.c g_settings[]).
 * ashford_game_bridge_init() rebases those pointers to librecvx_game.so in-place. */
#ifdef ASHFORD_ANDROID_GAME_BRIDGE
#undef g_appear_chr
#undef g_cheat
#undef g_enemy_opts
#undef g_lang
#undef g_lang_pictures
#undef g_lang_voices
#undef g_subtitles
#undef g_tps
#define g_appear_chr     ashford_sent_g_appear_chr
#define g_cheat          ashford_sent_g_cheat
#define g_enemy_opts     ashford_sent_g_enemy_opts
#define g_lang           ashford_sent_g_lang
#define g_lang_pictures  ashford_sent_g_lang_pictures
#define g_lang_voices    ashford_sent_g_lang_voices
#define g_subtitles      ashford_sent_g_subtitles
#define g_tps            ashford_sent_g_tps
#endif
