/* Android arm64 fixed-address window loader and DMA host pool.
 *
 * Single source of truth for the Android arm64 32-bit-compatible address map:
 *   [0x33000000, 0x40000000) game data window (208 MiB inside 0x3xxxxxxx, clear of ART dalvik-main [0x12c00000, 0x32c00000))
 *   [0x40000000, 0x50000000) game code range (.plt/.text/.eh_frame)
 *   [0x50000000, 0x70000000) DMA host pool guard range (256 MiB reserved at [0x50000000, 0x60000000))
 *   [0x70000000, 0x70004000) PS2 scratchpad (PS2_SPR_BASE / PS2_SPR_SIZE)
 */
#ifndef ASHFORD_WINDOW_LOAD_H
#define ASHFORD_WINDOW_LOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASHFORD_RESERVE_BASE      UINT32_C(0x33000000) /* reservation covers window + code, clear of ART 512 MiB heap */
#define ASHFORD_RESERVE_SIZE      UINT32_C(0x1D000000) /* [0x33000000, 0x50000000) */
#define ASHFORD_WINDOW_BASE       UINT32_C(0x33000000) /* game data + PS2 identities (all in 0x3xxxxxxx) */
#define ASHFORD_WINDOW_LIMIT      UINT32_C(0x40000000)
#define ASHFORD_CODE_BASE         UINT32_C(0x40000000) /* game code range */
#define ASHFORD_CODE_LIMIT        UINT32_C(0x50000000)
#define ASHFORD_POOL_BASE         UINT32_C(0x50000000) /* dma host pool */
#define ASHFORD_POOL_LIMIT        UINT32_C(0x70000000)
#define ASHFORD_POOL_RESERVE_SIZE UINT32_C(0x10000000) /* 256 MiB reserved at [0x50000000, 0x60000000) */
#define ASHFORD_POOL_SLOTS        4
#define ASHFORD_POOL_SLOT_SIZE    (ASHFORD_POOL_RESERVE_SIZE / ASHFORD_POOL_SLOTS)

#if defined(__ANDROID__)
int         ashford_window_init(void);            /* 0 ok; reserves both ranges, dlext-loads librecvx_game.so */
void*       ashford_game_handle(void);            /* dlopen handle after init, else NULL */
const char* ashford_game_lib_path(void);          /* resolved path (or apk!/entry) of loaded librecvx_game.so */
void*       ashford_pool_alloc(int slot, uint32_t len); /* one block per slot, replaces the slot's last; NULL outside the pool */
void        ashford_pool_free(int slot);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASHFORD_WINDOW_LOAD_H */
