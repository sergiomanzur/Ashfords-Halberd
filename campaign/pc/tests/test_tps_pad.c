#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "../src/host/tps.h"
#include <stdio.h>
#include <string.h>
static SYS_WORK state;
SYS_WORK* sys=&state;
BH_PWORK ply;
BH_PWORK* plp=&ply;
TpsConfig g_tps;
int tps_cam_active=1;
int tps_on=1;
static int pad_cam, pad_steer, pad_move, recenter, swings, stick_calls, fails;
static unsigned move_keys, keys;
static unsigned toggle_closes(void) {return 0;}
static int pad_sticks(unsigned int* pad) {stick_calls++;*pad=1;return 0;}
unsigned tps_input_keys(int quickturn) {(void)quickturn;return keys;}
static int in_scope(void) {return 0;}
static int lock_aim(void) {return 0;}
#include "tps_pad_test.inc"
#define CHECK(c) do {if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
int main(void)
{
    /* A native puzzle owns the directions, including the free-camera keyboard. */
    sys->sp_flg=0x40;g_tps.free_cam=1;
    CHECK(tps_pad(0x8)==0x8 && stick_calls==0);
    keys=TPS_K_RIGHT;CHECK(tps_pad(0)==0x8);
    keys=TPS_K_LEFT;CHECK(tps_pad(0)==0x4);
    /* Normal play still translates camera-relative movement and run. */
    sys->sp_flg=0x41;keys=TPS_K_FWD|TPS_K_RUN;
    CHECK((tps_pad(0)&0x401)==0x401 && stick_calls>0);
    ply.psh_ct=1;CHECK((tps_pad(0)&0x401)==0x1);
    /* First person ignores the third-person orbit preference: lateral keys
       become camera-relative movement, rather than rotating the view. */
    ply.psh_ct=0;tps_on=2;g_tps.free_cam=0;keys=TPS_K_RIGHT;
    CHECK((tps_pad(0)&0xf)==1 && move_keys==TPS_K_RIGHT);
    printf("third-person input ownership: %d failures\n",fails);
    return fails!=0;
}
