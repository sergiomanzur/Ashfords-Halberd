/* Third-person mode input: mouse (relative mode, main thread) and the keyboard layout
   (read on the game thread like pad.c). docs/THIRDPERSON.md */
#include "../host/tps.h"
#include "../host/menu_mouse.h"
#include "../host/bindings.h"
#include "../host/settings.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>

int pad_game_input_enabled(void);
extern SDL_Window* g_window;

/* Sample on the game thread before choosing the camera for this frame. A chord
   held across an overlay or focus transition must be released before reuse. */
void tps_camera_hotkeys(void)
{
    static int previous, suppressed, was_enabled = 1;
    int enabled = pad_game_input_enabled() && (g_input_source == INPUT_PAD ||
        (g_window && (SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS)));
    int held = bind_gamepad() && bind_held(B_FPS_TOGGLE, BIND_PAD, bind_keys(), 0);
    if (!enabled) {
        suppressed = held;
        was_enabled = 0;
        previous = held;
        return;
    }
    if (!was_enabled) suppressed |= held;
    was_enabled = 1;
    suppressed &= held;
    if (held && !previous && !suppressed) {
        g_tps.on = g_tps.on == 2 ? 0 : 2;
        settings_save_later();
    }
    previous = held;
}

static int mouse_x, mouse_y;   /* 1/64 pixel, atomic */
static int buttons;            /* SDL button mask, atomic */
static int relative;

void tps_input_event(const void* e)
{
    const SDL_Event* ev = (const SDL_Event*)e;
    if (!relative) return;
    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
        __atomic_add_fetch(&mouse_x, (int)(ev->motion.xrel * 64.0f), __ATOMIC_RELAXED);
        __atomic_add_fetch(&mouse_y, (int)(ev->motion.yrel * 64.0f), __ATOMIC_RELAXED);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        __atomic_or_fetch(&buttons, SDL_BUTTON_MASK(ev->button.button), __ATOMIC_RELAXED);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        __atomic_and_fetch(&buttons, ~SDL_BUTTON_MASK(ev->button.button), __ATOMIC_RELAXED);
        break;
    }
}

/* the cursor is captured while the window has focus, third person is on, the mouse plays (not
   "Gamepad only") and no overlay window or menu with mouse support (menu_mouse.c) is open */
void tps_update_mouse_mode(void* window, int overlay_open)
{
    int want = g_tps.on && !overlay_open && !menu_mouse_cursor() && g_input_source != INPUT_PAD
               && (SDL_GetWindowFlags((SDL_Window*)window) & SDL_WINDOW_INPUT_FOCUS);   /* alt-tab releases */
    if (want == relative) return;
    relative = want;
    SDL_SetWindowRelativeMouseMode((SDL_Window*)window, want);
    if (!want) __atomic_store_n(&buttons, 0, __ATOMIC_RELAXED);
}

int tps_mouse_captured(void) { return relative; }

/* Test automation: RECVX_TPS_MOUSE="<frame>:<dx>:<dy>,..." moves the mouse by dx/dy pixels
   per game frame from that presented frame until the next entry. */
uint64_t gs_frame_count(void);
static void scripted_mouse(float* dx, float* dy)
{
    static const char* script; static int checked;
    char* e;
    if (!checked++) script = SDL_getenv("RECVX_TPS_MOUSE");
    for (const char* p = script; p && *p; p = *e == ',' ? e + 1 : "") {
        unsigned long at = strtoul(p, &e, 10);
        float x = *e == ':' ? strtof(e + 1, &e) : 0, y = *e == ':' ? strtof(e + 1, &e) : 0;
        if (at > gs_frame_count()) break;
        *dx = x;
        *dy = y;
    }
}

void tps_take_mouse(float* dx, float* dy)
{
    *dx = __atomic_exchange_n(&mouse_x, 0, __ATOMIC_RELAXED) / 64.0f;
    *dy = __atomic_exchange_n(&mouse_y, 0, __ATOMIC_RELAXED) / 64.0f;
    if (!pad_game_input_enabled()) *dx = *dy = 0;
    scripted_mouse(dx, dy);
}

/* the bindings' third-person rows (bindings.c; by default W/S forward/back, A/D turn, Shift run,
   E/Space action, Q cancel, Tab status, M map, right mouse ready weapon, left mouse fire), and the
   gamepad's in third-person play with the modern scheme (they read nothing otherwise) */
unsigned tps_input_keys(int quickturn)
{
    static const struct { int row; unsigned bit; } map[] = {
        { B_TPS_FWD, TPS_K_FWD }, { B_TPS_BACK, TPS_K_BACK }, { B_TPS_LEFT, TPS_K_LEFT }, { B_TPS_RIGHT, TPS_K_RIGHT },
        { B_TPS_RUN, TPS_K_RUN }, { B_TPS_ACTION, TPS_K_ACTION }, { B_TPS_CANCEL, TPS_K_CANCEL },
        { B_TPS_STATUS, TPS_K_STATUS }, { B_TPS_MAP, TPS_K_MAP }, { B_TPS_AIM, TPS_K_AIM }, { B_TPS_FIRE, TPS_K_FIRE },
        { B_TPS_RECENTER, TPS_K_RECENTER }, { B_TPS_OPTIONS, TPS_K_OPTIONS },
    };
    const bool* k = bind_keys();
    int b = __atomic_load_n(&buttons, __ATOMIC_RELAXED), qt, pad = bind_gamepad() != NULL;
    unsigned r = 0;
    if (!pad_game_input_enabled()) return 0;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (bind_held(map[i].row, BIND_KB, k, b) || (pad && bind_held(map[i].row, BIND_PAD, k, 0))) r |= map[i].bit;
    qt = bind_quickturn(1, quickturn || bind_held(B_TPS_QUICKTURN, BIND_KB, k, b) || (pad && bind_held(B_TPS_QUICKTURN, BIND_PAD, k, 0)));
    if (qt) r |= TPS_K_BACK | TPS_K_QUICKTURN | (qt > 1 ? TPS_K_RUN : 0);
    return r;
}

static float axis(SDL_GamepadAxis a) { return SDL_GetGamepadAxis((SDL_Gamepad*)bind_gamepad(), a) / 32767.0f; }

int tps_pad_sticks(float* lx, float* ly, float* rx, float* ry)
{
    *lx = *ly = *rx = *ry = 0;
    if (!bind_gamepad() || !pad_game_input_enabled()) return 0;
    *lx = axis(SDL_GAMEPAD_AXIS_LEFTX); *ly = -axis(SDL_GAMEPAD_AXIS_LEFTY);
    *rx = axis(SDL_GAMEPAD_AXIS_RIGHTX); *ry = -axis(SDL_GAMEPAD_AXIS_RIGHTY);
    return 1;
}

void tps_pad_dpad(float* x, float* y)
{
    SDL_Gamepad* g = (SDL_Gamepad*)bind_gamepad();
    *x = *y = 0;
    if (!g || !pad_game_input_enabled()) return;
    *x = (float)(SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) - SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
    *y = (float)(SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP) - SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN));
}

/* the gamepad played this frame: a button, a trigger or a stick past a third */
int tps_pad_touched(void)
{
    SDL_Gamepad* g = (SDL_Gamepad*)bind_gamepad();
    if (!g || !pad_game_input_enabled()) return 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) if (SDL_GetGamepadButton(g, (SDL_GamepadButton)b)) return 1;
    for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++) if (SDL_abs(SDL_GetGamepadAxis(g, (SDL_GamepadAxis)a)) > 11000) return 1;
    return 0;
}

int tps_kbm_touched(void)
{
    const bool* k = bind_keys();
    if (!pad_game_input_enabled() || !input_accept(g_input_source, 0, BIND_KB, 0)) return 0;
    if (__atomic_load_n(&buttons, __ATOMIC_RELAXED)) return 1;
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++) if (k[i]) return 1;
    return 0;
}

/* mouse buttons held while the mouse is captured (bindings to mouse buttons) */
int tps_mouse_buttons(void) { return __atomic_load_n(&buttons, __ATOMIC_RELAXED); }
