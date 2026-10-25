/* Private POD state: no environment cursors, UI requests or host pointers. */
#ifndef ASHFORD_CHEAT_ROOM_SNAPSHOT_H
#define ASHFORD_CHEAT_ROOM_SNAPSHOT_H
#include <stdint.h>
#define CHEAT_ROOM_SITES 16
#define CHEAT_ROOM_SITE_WRITES 12
typedef struct { unsigned int attr, prm; unsigned char type; } CheatRoomValue;
typedef struct {
    unsigned int ifel, then0, els, end;
    int part, n;
    struct { unsigned char kind, ix, set[2]; CheatRoomValue pre, val[2]; } w[CHEAT_ROOM_SITE_WRITES];
} CheatRoomSite;
typedef struct {
    uint32_t version, bytes;
    CheatRoomSite sites[CHEAT_ROOM_SITES];
    int nsite, recording, evaluating, unlock_applied;
    int noclip_flew, noclip_base, noclip_room;
    int unlock_item, unlock_sb, unlock_live, unlock_hit, unlock_st8;
    unsigned char unlock_turn;
    int warp_sound, warp_bgm, warp_vol, warp_bgse[2];
} CheatRoomSnapshot;
int cheat_room_snapshot_capture(CheatRoomSnapshot* out);
int cheat_room_snapshot_validate(const CheatRoomSnapshot* in);
void cheat_room_snapshot_restore(const CheatRoomSnapshot* in); /* validated, no allocation */
#endif
