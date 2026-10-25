/* Production quicksave sampling and device discovery, real bindings, and an SDL virtual pad.
   Only focus/keyboard snapshots are controlled by this harness. */
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
static int playing = 1;
int reload_combo_ok(void) { return playing; }
static int tps_mouse_buttons(void) { return 0; }
static const bool* test_quicksave_keys(void) { return keys; }
static SDL_WindowFlags test_quicksave_flags(SDL_Window* window) { (void)window; return flags; }
static SDL_AtomicInt pads_changed;
#define bind_keys test_quicksave_keys
#define SDL_GetWindowFlags test_quicksave_flags
#include "pad_quicksave_test.inc"
#undef SDL_GetWindowFlags
#undef bind_keys

#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));
    bind_reset(BIND_KB); bind_reset(BIND_PAD);
    keys[SDL_SCANCODE_F9] = 1;
    CHECK(pad_quicksave_actions() == 1 && pad_quicksave_actions() == 1);
    keys[SDL_SCANCODE_F10] = 1; CHECK(pad_quicksave_actions() == 3);
    input_enabled = 0; CHECK(pad_quicksave_actions() == 0);
    input_enabled = 1; CHECK(pad_quicksave_actions() == 0);
    memset(keys, 0, sizeof(keys)); CHECK(pad_quicksave_actions() == 0);
    flags = 0; CHECK(pad_quicksave_actions() == 0);
    flags = SDL_WINDOW_INPUT_FOCUS; keys[SDL_SCANCODE_F9] = 1;
    CHECK(pad_quicksave_actions() == 0 && pad_quicksave_actions() == 0);
    keys[SDL_SCANCODE_F9] = 0; CHECK(pad_quicksave_actions() == 0);
    bind_set(B_QUICKLOAD, BIND_KB, 1, SDL_SCANCODE_K);
    keys[SDL_SCANCODE_K] = 1; CHECK(pad_quicksave_actions() == 2);
    memset(keys, 0, sizeof(keys)); CHECK(pad_quicksave_actions() == 0);

    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD; d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    gamepad = SDL_OpenGamepad(id); CHECK(gamepad); bind_set_gamepad(gamepad);
    SDL_Joystick* j = SDL_GetGamepadJoystick(gamepad);
    for (int modern = 0; modern < 2; ++modern) {
        g_tps.pad_play = modern;
        for (int load = 0; load < 2; ++load) {
            const int button = load ? SDL_GAMEPAD_BUTTON_START : SDL_GAMEPAD_BUTTON_BACK;
            SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
            SDL_SetJoystickVirtualButton(j, button, true); SDL_UpdateGamepads();
            /* Read native menus first: chord ownership cannot depend on hotkey polling order. */
            CHECK(!bind_held(B_SELECT, BIND_PAD, keys, 0) && !bind_held(B_START, BIND_PAD, keys, 0));
            CHECK(!bind_held(B_TPS_MAP, BIND_PAD, keys, 0) && !bind_held(B_TPS_OPTIONS, BIND_PAD, keys, 0));
            CHECK(pad_quicksave_actions() == (load ? 2u : 1u));
            SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, false); SDL_UpdateGamepads();
            CHECK(pad_quicksave_actions() == 0);
            CHECK(!bind_held(B_SELECT, BIND_PAD, keys, 0) && !bind_held(B_START, BIND_PAD, keys, 0));
            playing = 0; /* No late map/options when entering a blocked mode while held. */
            CHECK(!bind_held(B_TPS_MAP, BIND_PAD, keys, 0) && !bind_held(B_TPS_OPTIONS, BIND_PAD, keys, 0));
            SDL_SetJoystickVirtualButton(j, button, false); SDL_UpdateGamepads();
            CHECK(pad_quicksave_actions() == 0);
            (void)bind_held(B_SELECT, BIND_PAD, keys, 0); (void)bind_held(B_START, BIND_PAD, keys, 0);
            playing = 1;
        }
    }
    g_input_source = INPUT_PAD; flags = 0;
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_BACK, true); SDL_UpdateGamepads();
    CHECK(pad_quicksave_actions() == 1);
    input_enabled = 0; CHECK(pad_quicksave_actions() == 0);
    input_enabled = 1; CHECK(pad_quicksave_actions() == 0);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_BACK, false); SDL_UpdateGamepads();
    CHECK(pad_quicksave_actions() == 0);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_BACK, true); SDL_UpdateGamepads();
    CHECK(pad_quicksave_actions() == 1);
    g_input_source = INPUT_KBM; CHECK(pad_quicksave_actions() == 0);
    SDL_CloseGamepad(gamepad); bind_set_gamepad(NULL); SDL_DetachVirtualJoystick(id); SDL_Quit();
    puts("quicksave input gates: ok");
    return 0;
}
