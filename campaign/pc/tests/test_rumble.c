/* Actual libpad motor conversion against SDL virtual rumble-capable devices. */
#include "../src/ps2_platform.h"
#include "../src/host/bindings.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <stdio.h>
static SDL_Gamepad* gamepad;
SDL_Window* g_window;
HostConfig g_config;
int g_input_source = INPUT_PAD;
static int calls, fails;
static Uint16 low, high;
static void ensure_gamepad(void) {}
uint64_t gs_frame_count(void) { return 0; }
void host_log(const char* fmt, ...) { (void)fmt; }
#include "rumble_test.inc"
static bool SDLCALL motors(void* userdata, Uint16 lo, Uint16 hi)
{ (void)userdata; low=lo;high=hi;calls++;return true; }
#define CHECK(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} } while(0)
int main(void)
{
    SDL_VirtualJoystickDesc desc;
    unsigned char data[6]={1,255};
    if(!SDL_Init(SDL_INIT_GAMEPAD)) {printf("SDL init: %s\n",SDL_GetError());return 1;}
    CHECK(!pad_can_rumble());
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_MISC1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_MISC1)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.vendor_id=0x045e;desc.product_id=0x02ea;desc.name="Ashford virtual pad";
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    gamepad=SDL_OpenGamepad(id);CHECK(gamepad && !pad_can_rumble());
    SDL_CloseGamepad(gamepad);gamepad=NULL;SDL_DetachVirtualJoystick(id);
    desc.Rumble=motors;
    id=SDL_AttachVirtualJoystick(&desc);gamepad=SDL_OpenGamepad(id);
    CHECK(gamepad && pad_can_rumble());
    g_config.rumble=100;scePadSetActDirect(0,0,data);CHECK(low==65535 && high==0xc000);
    g_config.rumble=50;scePadSetActDirect(0,0,data);CHECK(low==32767 && high==0x6000);
    g_config.rumble=0;scePadSetActDirect(0,0,data);CHECK(low==0 && high==0);
    g_config.rumble=100;g_input_source=INPUT_AUTO;
    scePadSetActDirect(0,0,data);CHECK(low==0 && high==0); /* no focused window */
    int before=calls;scePadSetActDirect(1,0,data);CHECK(calls==before);
    SDL_CloseGamepad(gamepad);gamepad=NULL;SDL_DetachVirtualJoystick(id);SDL_Quit();
    printf("rumble capability and motors: %d failures\n",fails);
    return fails!=0;
}
