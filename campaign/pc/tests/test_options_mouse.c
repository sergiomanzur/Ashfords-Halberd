#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/adv.h>
#include <ps2/veronica/prog/padman.h>
#include "../src/host/menu_mouse.h"
#include "../src/host/bindings.h"
#include "../src/host/tps.h"
#include <stdio.h>
#include <string.h>
ADV_WORK AdvWork;
PAD_WRK Pad[4];
TpsConfig g_tps;
static MouseTake mt, pending;
static int opt_held, rumble_available, sounds, checked_port;
enum { P_SELECT=0x100, P_LEFT=0x8000, P_RIGHT=0x2000 };
int bind_pad_raw(int row) { (void)row; return 0; }
int pc_in_game(void) { return 0; }
void menu_mouse_take(MouseTake* to) { *to = pending; }
static float row8_x(void) { return 350; }
static float row9_x(void) { return 350; }
void CallSystemSe(int param, int se) { (void)param; (void)se; sounds++; }
static unsigned int ok_button(void) { return 0x40; }
static unsigned int cancel_button(void) { return 0x20; }
int CheckVibrationUnit(unsigned int port) { checked_port=port; return rumble_available; }
#include "options_mouse_test.inc"
static int fails;
#define CHECK(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} } while(0)
int main(void)
{
    pending.x=80; pending.y=304; pending.moved=1;
    for(int frame=0;frame<100;frame++) { AdvWork.OptIndex=5; pc_options_mouse0(0); }
    CHECK(AdvWork.OptIndex==5 && sounds==0);
    pending.left=1; pending.x=450; pc_options_mouse0(0);
    CHECK(Pad[0].press==0 && sounds==0);
    rumble_available=1; pending.x=80; pending.left=0;
    pc_options_mouse0(0); CHECK(AdvWork.OptIndex==4 && sounds==1 && checked_port==2);
    pc_options_mouse0(0); CHECK(sounds==1);
    pending.x=500; pending.left=1; pc_options_mouse0(0);
    CHECK(Pad[0].press & P_RIGHT);
    rumble_available=0; pending.left=0; pending.x=80; pending.y=176;
    pc_options_mouse0(0); CHECK(AdvWork.OptIndex==0 && sounds==2);
    printf("option mouse: %d failures\n",fails);
    return fails!=0;
}
