/* LP64 on-disc binary struct realization (docs/android-backend.md "LP64").
 *
 * The PS2 game data (room files, model binaries) embeds 32-bit structures whose
 * pointer fields are 4 bytes. The 32-bit Windows build overlays them in place
 * because the host layout matches the disc layout. On LP64 Android (arm64)
 * every pointer field is 8 bytes, so those structures must be UNPACKED into
 * host-layout copies inside a resettable arena before the game touches them;
 * the pristine disc bytes are never mutated (the 32-bit path's in-place
 * sentinel clearing stays 32-bit-only).
 *
 * The arena lives in this translation unit's BSS inside librecvx_game.so,
 * which android_dlopen_ext() loads into the fixed game data window
 * [0x33000000, 0x40000000); the .ahsnap manifest covers it through
 * SAFE_GLUE (campaign/pc/tools/snapshot_manifest.py).
 *
 * Everything here compiles to a zero-cost pass-through on 32-bit hosts
 * (UINTPTR_MAX == 0xFFFFFFFF): the hooks return 0 and the decomp runs its
 * original in-place code unchanged.
 */
#ifndef ASHFORD_LP64_REALIZE_H
#define ASHFORD_LP64_REALIZE_H

#include <stdint.h>
#include <stddef.h>

/* The 32-bit disc layout of ROM_WORK (fixed by the PS2 disc format; it does
   NOT follow the host struct). Used by the PC glue to read room files on any
   host, so the reads below replace `((const ROM_WORK*)disc)->field` casts. */
#define LP64_RO_OFF_CUTP 0x000u
#define LP64_RO_OFF_LGTP 0x004u
#define LP64_RO_OFF_ENEP 0x008u
#define LP64_RO_OFF_OBJP 0x00cu
#define LP64_RO_OFF_ITMP 0x010u
#define LP64_RO_OFF_EFFP 0x014u
#define LP64_RO_OFF_WALP 0x018u
#define LP64_RO_OFF_ETCP 0x01cu
#define LP64_RO_OFF_FLRP 0x020u
#define LP64_RO_OFF_POSP 0x024u
#define LP64_RO_OFF_RUTP 0x028u
#define LP64_RO_OFF_RUTTP 0x02cu
#define LP64_RO_OFF_EVTP 0x030u
#define LP64_RO_OFF_EVCP 0x034u
#define LP64_RO_OFF_MESP 0x038u
#define LP64_RO_OFF_EVLP 0x03cu
#define LP64_RO_OFF_CUT_N 0x080u
#define LP64_RO_OFF_LGT_N 0x084u
#define LP64_RO_OFF_ENE_N 0x088u
#define LP64_RO_OFF_OBJ_N 0x08cu
#define LP64_RO_OFF_ITM_N 0x090u
#define LP64_RO_OFF_EFF_N 0x094u
#define LP64_RO_OFF_EVC_N 0x0acu
#define LP64_RO_OFF_MES_N 0x0b0u
#define LP64_RO_OFF_EVL_N 0x0b4u
#define LP64_ROM_DISC_SIZE 0x3ECu

/* Unaligned little-endian disc word. */
static inline uint32_t lp64_rdu32(const unsigned char* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Read one 32-bit ROM_WORK field straight off the disc bytes. Identical to
   the old `((const ROM_WORK*)disc)->field` on 32-bit hosts (same offsets),
   and layout-true on LP64. */
static inline uint32_t lp64_rom_field(const unsigned char* rom_disc, uint32_t off)
{
    return lp64_rdu32(rom_disc + off);
}

/* Arena slots. ROOM is reset at every room load; SESSION (player, weapons,
   item-view models) lives for the whole game session. */
enum { LP64_SLOT_ROOM = 0, LP64_SLOT_SESSION = 1, LP64_SLOT_COUNT = 2 };

/* All hooks return 0 on 32-bit hosts (the caller runs the original code). */
int      lp64_is_on(void);
void     lp64_reset(int slot);   /* ROOM: room load; SESSION: never during a game */

/* bhInitRoom: unpacks ROM_WORK in place (field-exact), the CUT_WORK, LGT_WORK
   (lgtp and evlp) and EVC_WORK arrays into the room arena, and realizes
   rom->evtp as a 1-entry EVT_WORK view (the disc block's remaining words are
   per-event offsets, read through lp64_evt_offset). Returns 1 when handled. */
struct ROM_WORK;
int      lp64_room_rom(struct ROM_WORK* rom, const unsigned char* rdtp);
int      lp64_active(void);   /* 1 after a handled lp64_room_rom (this room) */

/* Event_init: the per-event byte offset stored after the EVT_WORK block's
   first three words on disc. On 32-bit hosts this reads the exact same word
   the original `((int*)rom->evtp)[evt_id + 2]` read. */
ptrdiff_t lp64_evt_offset(unsigned evt_id);

/* bhMlbBinRealize / bhMnbBinRealize equivalents: unpack the Ninja binary
   (object tree, models, meshsets, texlist/texnames, motion + MDATA2 array)
   into the slot's arena with 0xFFFFFFFF sentinels realized as the original
   code would, deduplicated by disc base. Return 1 when handled. */
struct ML_WORK;
struct MN_WORK;
int      lp64_mlb(const void* bin_datP, struct ML_WORK* mlwP, int slot);
int      lp64_mnb(const void* bin_datP, struct MN_WORK* mnwP, int slot);

/* The glue's model swapping (actor_glue, placer_glue, variant_glue) realizes models from its own 32-bit
   copies of the disc data: lp64_*_soft unpack them like the decomp's hooks, but leave the work empty
   (objP NULL) instead of aborting when the slot is nearly full. 0 on 32-bit hosts. */
int      lp64_mlb_soft(const void* bin_datP, struct ML_WORK* mlwP, int slot);
int      lp64_mnb_soft(const void* bin_datP, struct MN_WORK* mnwP, int slot);
#define GLUE_MLB(p, m, slot) do { if (!lp64_mlb_soft((p), (m), (slot))) bhMlbBinRealize((p), (m)); } while (0)
#define GLUE_MNB(p, m, slot) do { if (!lp64_mnb_soft((p), (m), (slot))) bhMnbBinRealize((p), (m)); } while (0)

/* Test/introspection: current bounds of a slot's arena. */
void     lp64_arena_bounds(int slot, const void** lo, const void** hi);

#endif /* ASHFORD_LP64_REALIZE_H */
