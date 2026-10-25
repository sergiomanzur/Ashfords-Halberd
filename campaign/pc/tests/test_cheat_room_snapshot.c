/* Extract complete production helpers and pc_noclip with control_source.py.
   This fixture provides only the engine fields touched by the landing routine. */
#include "host/cheat_room_snapshot.h"
#include "host/cheat.h"
#include <stdio.h>
#include <string.h>
#define SITES CHEAT_ROOM_SITES
#define SITE_W CHEAT_ROOM_SITE_WRITES
enum { W_RM, W_ETC, W_ETC_ON, W_FLR, W_FLR_ON };
typedef CheatRoomSite Site;
static Site site[SITES];
static int nsite,recording,evaluating,unlock_applied;
static int noclip_flew,noclip_base,noclip_room;
static int unlock_item,unlock_sb,unlock_live,unlock_hit,unlock_st8;
static unsigned char unlock_turn;
static int warp_sound,warp_bgm,warp_vol,warp_bgse[2];
CheatState g_cheat;
typedef struct { int mode0,flr_no;float py; } Player;
static Player ply,*plp=&ply;
static struct { float grand[258]; } room_data,*rom=&room_data;
static int room_key=1,vertical=1,floor_updates;
static int pc_room_key(void) { return room_key; }
static int pad_noclip_vertical(void) { return vertical; }
static void bhSetFloorNum(Player* p) { (void)p;++floor_updates; }
#include "cheat_room_test.inc"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1; } } while (0)
int main(void)
{
    CheatRoomSnapshot saved,other,restored;
    nsite=2;recording=0;unlock_applied=1;unlock_turn=7;
    site[0].ifel=10;site[0].then0=20;site[0].els=40;site[0].end=50;site[0].part=1;site[0].n=1;
    site[0].w[0].kind=W_FLR;site[0].w[0].ix=3;site[0].w[0].set[1]=1;site[0].w[0].val[1].attr=0x400000;
    site[1].ifel=64;site[1].then0=70;site[1].end=80;site[1].part=-1;
    warp_bgm=44;warp_vol=-700;warp_bgse[0]=6;warp_bgse[1]=7;
    ply.mode0=1;ply.flr_no=3;ply.py=10;rom->grand[5]=123;rom->grand[9]=999;
    g_cheat.noclip=2;CHECK(pc_noclip()==0x18);CHECK(ply.py==11);
    CHECK(cheat_room_snapshot_capture(&saved));CHECK(saved.noclip_flew && saved.noclip_base==3 && saved.noclip_room==1);
    /* Visit another room and overwrite its unlock records and take-off floor. */
    room_key=2;g_cheat.noclip=0;pc_noclip();ply.flr_no=7;g_cheat.noclip=2;pc_noclip();
    memset(site,0,sizeof(site));nsite=0;unlock_applied=0;unlock_turn=99;warp_bgm=99;
    CHECK(cheat_room_snapshot_capture(&other));CHECK(other.noclip_base==7);
    cheat_room_snapshot_restore(&saved);CHECK(cheat_room_snapshot_capture(&restored));
    CHECK(!memcmp(&saved,&restored,sizeof(saved)));
    CHECK(nsite==2 && site[0].w[0].ix==3 && site[0].w[0].val[1].attr==0x400000 && unlock_turn==7 && warp_bgm==44);
    room_key=1;g_cheat.noclip=0;ply.py=200;CHECK(pc_noclip()==0);CHECK(ply.py==123 && !noclip_flew);
    evaluating=1;CHECK(!cheat_room_snapshot_capture(&other));evaluating=0;
    unlock_live=1;CHECK(!cheat_room_snapshot_capture(&other));unlock_live=0;
    other=saved;other.nsite=SITES+1;CHECK(!cheat_room_snapshot_validate(&other));
    other=saved;other.sites[0].w[0].kind=99;CHECK(!cheat_room_snapshot_validate(&other));
    other=saved;other.sites[0].w[0].set[1]=2;CHECK(!cheat_room_snapshot_validate(&other));
    other=saved;other.sites[0].then0=1;CHECK(!cheat_room_snapshot_validate(&other));
    CHECK(floor_updates>=3);puts("cheat room snapshot: cross-room POD and noclip landing passed");return 0;
}
