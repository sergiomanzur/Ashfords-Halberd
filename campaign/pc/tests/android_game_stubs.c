/* Hidden-visibility fallback definitions for game-side symbols referenced by
 * recvx_platform (overlay/settings/adx/host) so Android headless test harnesses
 * can statically link recvx_platform with --export-dynamic without pulling
 * librecvx_game.so into DT_NEEDED (which would load it at ASLR before
 * ashford_window_init() runs).
 *
 * Because every symbol here has hidden visibility (and librecvx_game.so links
 * with -Bsymbolic), none of these are exported in .dynsym and librecvx_game.so
 * always resolves game symbols to its own windowed copies in [0x30000000, 0x50000000).
 */
#if defined(__ANDROID__)
#define H __attribute__((visibility("hidden")))

/* Data objects referenced by platform layer */
H unsigned char g_appear_chr[64];
H unsigned char g_hud[256];
H unsigned char g_lang[64];
H unsigned char g_lang_pictures[64];
H unsigned char g_subtitles[64];
H unsigned char tps_cam_active[64];
H unsigned char comevt_message_tab[256];
H unsigned char fsheader[256];
H unsigned char fstbl[256];
H unsigned char g_cheat[4096];
H unsigned char g_enemy_opts[4096];
H unsigned char g_lang_voices[64];
H unsigned char g_tps[4096];
H unsigned char rdx_files[256];
H unsigned char wallpaper[256];
H const unsigned char FontSz[256];

/* Functions referenced by platform layer */
H void appear_frame(void) {}
H void biorand_frame(void) {}
H void cheat_frame(void) {}
H int  cheat_room_snapshot_capture(void) { return 0; }
H int  cheat_room_snapshot_restore(void) { return 0; }
H int  cheat_room_snapshot_validate(void) { return 0; }
H void enemy_opts(void) {}
H int  enemy_rate_at(void) { return 0; }
H int  enemy_rate_count(void) { return 0; }
H const char* enemy_rate_name(void) { return ""; }
H void enemy_rate_preset(void) {}
H void enemy_rates_ini(void) {}
H void enemy_rates_load(void) {}
H const char* enemy_rates_text(void) { return ""; }
H void glue_debug_frame(void) {}
H void hud_icons(void) {}
H void hud_item_info(void) {}
H void lang_frame(void) {}
H int  lang_has_subtitles(void) { return 0; }
H int  lang_has_voices(void) { return 0; }
H int  lang_installed(void) { return 0; }
H int  lang_old_import(void) { return 0; }
H void lang_scan(void) {}
H int  lang_sub(void) { return 0; }
H void pc_frame_fade(void) {}
H void pc_frame_view(void) {}
H int  pc_in_game(void) { return 0; }
H int  pc_insn_len_raw(void) { return 0; }
H int  pc_keytype(void) { return 0; }
H int  pc_keytype_mask(void) { return 0; }
H int  pc_lang_disc_parts(void) { return 0; }
H const char* pc_lang_file_dir(void) { return ""; }
H void pc_lang_file_opened(void) {}
H int  pc_menu_smooth(void) { return 0; }
H int  pc_rifle_stack(void) { return 0; }
H void pc_tex_invalidate(void) {}
H void pc_view_changed(void) {}
H int  recvx_game_main(int argc, char** argv) { (void)argc; (void)argv; return 0; }
H int  reload_combo_ok(void) { return 0; }
H int  room_snapshot_busy(void) { return 0; }
H const char* RoomNameSwitch(void) { return ""; }
H int  spawn_count(void) { return 0; }
H const char* spawn_name(void) { return ""; }
H void spoiler_key(void) {}
H void tps_frame(void) {}
#endif
