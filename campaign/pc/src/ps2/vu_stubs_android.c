/* Android arm64 windowed definitions of the VU microcode DMA stub packets.
 *
 * Linked directly into librecvx_game.so so Ps2Vu0ProgSend/Ps2Vu1ProgSend pass
 * addresses in [0x30000000, 0x40000000) to sceDmaSend() rather than high 64-bit
 * host addresses from recvx_platform.
 */
#if defined(__ANDROID__)
#include <stdint.h>

const uint32_t ps2_vu0sub0[4] __attribute__((aligned(16), visibility("default"))) = { 0x70000000, 0, 0, 0 };
const uint32_t ps2_vu1sub0[4] __attribute__((aligned(16), visibility("default"))) = { 0x70000000, 0, 0, 0 };
const uint32_t ps2_vu1sub1[4] __attribute__((aligned(16), visibility("default"))) = { 0x70000000, 0, 0, 0 };
#endif
