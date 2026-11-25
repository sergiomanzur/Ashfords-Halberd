/* Android arm64 runtime bridge between libmain.so (platform layer) and
 * librecvx_game.so (fixed-window game + glue library).
 *
 * libmain.so must NOT list librecvx_game.so in DT_NEEDED, or bionic loads it
 * under ASLR before ashford_window_init() reserves [0x30000000, 0x50000000).
 * After ashford_window_init() loads librecvx_game.so at 0x30000000,
 * ashford_game_bridge_init() resolves all 16 game data pointers and 45 game
 * function pointers via dlsym() and rebases config/settings tables in-place.
 */
#ifndef ASHFORD_ANDROID_GAME_BRIDGE_H
#define ASHFORD_ANDROID_GAME_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ANDROID__) && defined(ASHFORD_ANDROID_GAME_BRIDGE)
typedef struct AshfordBridgeAudit {
    int ready;
    int data_count;
    int func_count;
    int data_in_window;
    int func_in_code;
} AshfordBridgeAudit;

void ashford_game_bridge_init(void* game_handle);
int  ashford_game_bridge_ready(void);
void ashford_game_bridge_audit(AshfordBridgeAudit* out);
void* ashford_game_bridge_rebase_ptr(void* p);

/* Rebase hooks implemented in config.c and settings.c. */
void config_android_bridge_rebase(void* (*rebase)(void*));
void settings_android_bridge_rebase(void* (*rebase)(void*));
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASHFORD_ANDROID_GAME_BRIDGE_H */
