/* The production pad_weapon_switch function, real bindings, and an SDL virtual pad.
   Only focus/keyboard snapshots and device discovery are controlled by this harness. */
#include "../src/host/bindings.h"
#include "../src/host/tps.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

TpsConfig g_tps;
SDL_Window* g_window = (SDL_Window*)1;
static bool keys[SDL_SCANCODE_COUNT];
static SDL_WindowFlags flags = SDL_WINDOW_INPUT_FOCUS;
static int input_enabled = 1;
static SDL_Gamepad* gamepad;
uint64_t gs_frame_count(void) { return 0; }
void host_log(const char* fmt, ...) { (void)fmt; }
int pc_keytype_mask(int v) { (void)v; return -1; }
int pc_keytype(void) { return 0; }
int reload_combo_ok(void) { return 0; }
static int tps_mouse_buttons(void) { return 0; }
static const bool* test_weapon_keys(void) { return keys; }
static SDL_WindowFlags test_weapon_flags(SDL_Window* window) { (void)window; return flags; }
static void ensure_gamepad(void) {}
#define bind_keys test_weapon_keys
#define SDL_GetWindowFlags test_weapon_flags
#include "pad_weapon_switch_test.inc"
#undef SDL_GetWindowFlags
#undef bind_keys

#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));
    bind_reset(BIND_KB); bind_reset(BIND_PAD);
    keys[SDL_SCANCODE_LEFTBRACKET] = keys[SDL_SCANCODE_F7] = 1;
    CHECK(pad_weapon_switch() == 33);
    CHECK(pad_weapon_switch() == 33); /* repeated sampling is held state */
    input_enabled = 0; CHECK(pad_weapon_switch() == 0);
    input_enabled = 1; CHECK(pad_weapon_switch() == 0); /* held through overlay */
    CHECK(pad_weapon_switch() == 0);
    keys[SDL_SCANCODE_LEFTBRACKET] = 0; CHECK(pad_weapon_switch() == 0);
    keys[SDL_SCANCODE_LEFTBRACKET] = 1; CHECK(pad_weapon_switch() == 1);
    memset(keys, 0, sizeof(keys)); CHECK(pad_weapon_switch() == 0);

    /* A press made while blocked is dropped too. Different bindings remain usable. */
    input_enabled = 0; keys[SDL_SCANCODE_F3] = 1; CHECK(pad_weapon_switch() == 0);
    input_enabled = 1; CHECK(pad_weapon_switch() == 0);
    keys[SDL_SCANCODE_F4] = 1; CHECK(pad_weapon_switch() == 8);
    memset(keys, 0, sizeof(keys)); CHECK(pad_weapon_switch() == 0);

    /* SDL can clear held keys on focus loss; the first focused sample suppresses them. */
    flags = 0; CHECK(pad_weapon_switch() == 0);
    flags = SDL_WINDOW_INPUT_FOCUS; keys[SDL_SCANCODE_RIGHTBRACKET] = 1;
    CHECK(pad_weapon_switch() == 0);
    CHECK(pad_weapon_switch() == 0);
    keys[SDL_SCANCODE_RIGHTBRACKET] = 0; CHECK(pad_weapon_switch() == 0);
    keys[SDL_SCANCODE_RIGHTBRACKET] = 1; CHECK(pad_weapon_switch() == 2);
    memset(keys, 0, sizeof(keys)); CHECK(pad_weapon_switch() == 0);
    bind_set(B_WEAPON_1, BIND_KB, 1, SDL_SCANCODE_K);
    keys[SDL_SCANCODE_K] = 1; CHECK(pad_weapon_switch() == 4);
    memset(keys, 0, sizeof(keys)); CHECK(pad_weapon_switch() == 0);

    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD; d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    gamepad = SDL_OpenGamepad(id); CHECK(gamepad);
    bind_set_gamepad(gamepad);
    /* Existing single-button remaps remain valid alongside the new defaults. */
    bind_set(B_WEAPON_NEXT, BIND_PAD, 0, 1 + SDL_GAMEPAD_BUTTON_LEFT_STICK);
    SDL_Joystick* j = SDL_GetGamepadJoystick(gamepad);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
    SDL_UpdateGamepads(); CHECK(pad_weapon_switch() == 2);
    input_enabled = 0; CHECK(pad_weapon_switch() == 0);
    input_enabled = 1; CHECK(pad_weapon_switch() == 0);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, false);
    SDL_UpdateGamepads(); CHECK(pad_weapon_switch() == 0);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
    SDL_UpdateGamepads(); CHECK(pad_weapon_switch() == 2);

    /* Documented Gamepad only mode plays in the background; keyboard remains filtered. */
    g_input_source = INPUT_PAD; flags = 0; keys[SDL_SCANCODE_K] = 1;
    CHECK(pad_weapon_switch() == 2);
    input_enabled = 0; CHECK(pad_weapon_switch() == 0);
    input_enabled = 1; CHECK(pad_weapon_switch() == 0);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, false);
    SDL_UpdateGamepads(); CHECK(pad_weapon_switch() == 0);
    bind_set(B_WEAPON_NEXT, BIND_PAD, 0, 0);
    bind_set(B_WEAPON_2, BIND_PAD, 1, 1 + SDL_GAMEPAD_BUTTON_LEFT_STICK);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
    SDL_UpdateGamepads(); CHECK(pad_weapon_switch() == 8);
    SDL_CloseGamepad(gamepad); bind_set_gamepad(NULL);
    SDL_DetachVirtualJoystick(id); SDL_Quit();
    puts("weapon input gates: ok");
    return 0;
}
