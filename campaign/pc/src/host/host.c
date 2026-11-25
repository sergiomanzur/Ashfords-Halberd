/*
 * Host entry point: configuration, SDL window/GL context, event pump, and the
 * hand-off into the original game's main() (renamed recvx_game_main).
 */
#include "ps2_platform.h"
#include "../gs/stream.h"
#include "prof.h"
#include "tps.h"
#include "menu_mouse.h"
#include "bindings.h"
#include "settings.h"
#include "biorand.h"
#include "lang.h"
#include "../import/import.h"
extern char g_lang_us_iso[1024];   /* import/lang.c */
#include "ashford_disc.h"
#include "ashford_launcher.h"
#include "quicksave.h"
#include "../gs/gs.h"
void render_loop(void);
#include <SDL3/SDL.h>
#if defined(__ANDROID__) && !defined(ASHFORD_HOST_NOMAIN)
#include <SDL3/SDL_main.h>
#endif
#if defined(__ANDROID__)
#include "window_load.h"
#include <android/log.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#if defined(__ANDROID__)
#include <unwind.h>
#else
#include <execinfo.h>
#endif
#endif

int recvx_game_main(int argc, char* argv[]);
void audio_init(void);
void gs_renderer_init(void);

int pc_screen_filter_mode(void) { return g_config.screen_filter; }
/* "Skip door animations" (decomp system.c bhSysCallDoordemo): 0 the animation, 1 instant (the
   load screen), 2 the animation until a key, mouse button or gamepad button is pressed */
int pc_skip_door_demo(void) { return g_config.skip_doors == 1; }
/* "Fast loading" (decomp system.c bhSysCallMonitor/bhSysCallSndMonitor, sdfunc.c pc_load_sound_pack_now) */
int pc_fast_loading(void) { return g_config.fast_loading; }
static SDL_AtomicInt presses;   /* key, mouse and gamepad button presses (host_pump_events) */
static SDL_AtomicInt movie_skip;
int pc_movie_skip_pressed(int start)
{
    int requested = SDL_SetAtomicInt(&movie_skip, 0);
    return start ? 0 : requested;
}
int pc_door_skip_pressed(int start)
{
    static int at;   /* game thread */
    int n = SDL_GetAtomicInt(&presses);
    if (start) at = n;   /* the demo begins: the press that opened the door is older */
    return g_config.skip_doors == 2 && n != at;
}
int pc_intro_pressed(int start)
{
    static int at;   /* game thread */
    int n = SDL_GetAtomicInt(&presses);
    if (start) at = n;   /* a screen begins: older presses (and held keys) do not count */
    return n != at;
}
int pc_quick_knife_on(void) { return g_config.quick_knife; }
int pc_head_blowoff_on(void) { return g_config.head_blowoff; }
int pc_auto_stairs_on(void) { return g_config.auto_stairs; }
int pc_cfg_no_ink_ribbon(void) { return g_config.no_ink_ribbon; }
int pc_cfg_extra_objects(void) { return g_config.extra_objects; }
/* the game tests a few joints of a model against the screen; with a wide or moving free
   camera that drops models that are partly visible, so third person never culls */
#ifdef ASHFORD_ANDROID_GAME_BRIDGE
__attribute__((visibility("hidden"))) extern int* ashford_ptr_tps_cam_active;
#define tps_cam_active (*ashford_ptr_tps_cam_active)
#else
extern int tps_cam_active;
#endif
int pc_model_cull(void) { return g_config.model_cull && !tps_cam_active; }
int pc_frame_blend(void) { return g_config.frame_blend < 0 ? !g_config.interpolate : g_config.frame_blend; }
int pc_battle_unlocked(void) { return g_config.battle_unlocked; }
int pc_difficulty_select(void) { return g_config.difficulty_select; }
int pc_skip_intro(void) { return g_config.skip_intro; }
int pc_movie_zoom(void) { return g_config.movie_zoom; }
int pc_menu_smooth_on(void) { return g_config.menu_smooth && g_config.interpolate; }   /* view_glue.c pc_menu_tag */
/* fog at i % of the original: the fog density (njCalcFogPower) x 100 / i, so everything's fog is
   that of something i % as far: 50 % = x 2. 0 %: every distance reads the table's nearest
   entry, 254 or 255 of 255 (no fog) */
float pc_fog_scale(void) { int i = g_config.fog_intensity; return i <= 0 ? 1e6f : 100.0f / i; }
int pc_skip_intro_press_start(void)
{
    static int done;
    if (!g_config.skip_intro || done) return 0;
    done = 1;
    return 1;
}
int pc_config_native_tex(void) { return g_config.native_tex; }
int pc_native_3d(void)
{
    static int env = -2;
    if (env == -2) env = getenv("RECVX_NATIVE3D") ? atoi(getenv("RECVX_NATIVE3D")) != 0 : -1;
    return env >= 0 ? env : g_config.native_3d;
}

/* full-screen 2D overlays: 0 original 4:3, 1 stretched to the (widened) view, 2 off;
   fades always cover the whole view */
enum { PC_OVL_CINEMA, PC_OVL_FADE, PC_OVL_EFFECT };   /* as in pc_prelude.h */
static int overlay_stretch;
int pc_overlay_begin(int kind)
{
    int mode = kind == PC_OVL_CINEMA ? g_config.cinema_bars : kind == PC_OVL_EFFECT ? g_config.screen_effects : 1;
    overlay_stretch = mode == 1;
    return mode != 2;   /* Off: the caller skips the layer, or (the oil pressure puzzle) draws it 4:3 */
}
void pc_overlay_end(void) { overlay_stretch = 0; }
int pc_overlay_wide(void) { return overlay_stretch; }
float pc_overlay_x(float x)
{
    extern float pc_hor_scale(void);
    return overlay_stretch ? 320.0f + (x - 320.0f) * pc_hor_scale() : x;
}

HostConfig g_config = { .window_w = 1280, .window_h = 960, .borderless_extra = 1,.vsync = 1, .log_level = 1, .music_volume = 100, .sfx_volume = 100, .rumble = 100, .interpolate = 1, .menu_smooth = 1, .frame_blend = 0, .aspect = 2, .show_fps = 1, .show_ping = 1,
                       .screen_filter = 1, .cinema_bars = 1, .screen_effects = 1, .native_3d = 1, .model_cull = 1, .native_tex = 1, .hd_textures = 1, .hd_textures_ui = 1, .quick_knife = 1, .head_blowoff = 1, .rifle_stack = 1,
                       .skip_doors = 1, .fast_loading = 1, .auto_stairs = 1, .skip_intro = 1, .battle_unlocked = 1, .difficulty_select = 1, .pause_menu = 1, .contrast = 100, .gamma = 10, .saturation = 100, .settings_preset = 2, .fog_intensity = 100 };   /* first-start defaults; recvx.ini overrides */
SDL_Window* g_window;
SDL_GLContext g_gl;
static int g_app_suspended;

int host_app_suspended(void) { return g_app_suspended; }

static void host_apply_gl_attributes(void)
{
#if defined(__ANDROID__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

static void host_enter_background(void)
{
    if (g_app_suspended) return;
    host_log("lifecycle: entering background\n");
    config_save();
    /* Request a safety auto-quicksave if the game is in a legal state, and
       briefly service the render-thread snapshot barrier while GL is alive. */
    if (g_gl) {
        uint64_t deadline = SDL_GetTicks() + 400;
        quicksave_request_auto();
        while (SDL_GetTicks() < deadline) {
            stream_snapshot_service();
            if (!quicksave_auto_pending()) break;
            SDL_Delay(2);
        }
        /* Let an already-started room_snapshot_capture finish its render barrier. */
        deadline = SDL_GetTicks() + 400;
        while (SDL_GetTicks() < deadline) {
            if (stream_snapshot_service() != 0) break;
            SDL_Delay(2);
        }
        (void)quicksave_take_auto(); /* clear if game thread was paused/blocked and did not consume */
    }
    overlay_context_lost(g_gl != NULL);
    gs_renderer_context_lost(g_gl != NULL);
    if (g_gl) {
        SDL_GL_DestroyContext(g_gl);
        g_gl = NULL;
    }
    g_app_suspended = 1;
}

static void host_enter_foreground(void)
{
    if (!g_app_suspended && g_gl) return;
    host_log("lifecycle: resuming foreground\n");
    if (!g_gl && g_window) {
        host_apply_gl_attributes();
        g_gl = SDL_GL_CreateContext(g_window);
        if (!g_gl) host_fatal("SDL_GL_CreateContext (resume): %s", SDL_GetError());
        SDL_GL_SetSwapInterval(g_config.vsync ? 1 : 0);
        gs_renderer_context_restore();
        overlay_context_restore(g_window, g_gl);
    }
    g_app_suspended = 0;
}

void log_keep(const char* fmt, va_list ap);   /* log.c: the last messages for the crash report */

void host_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_keep(fmt, ap);   /* also with --quiet: the crash report keeps the last messages */
    va_end(ap);
    if (g_config.log_level <= 0) return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#if defined(__ANDROID__)
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "AshfordsHalberd", fmt, ap);
    va_end(ap);
#endif
}

void host_fatal(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "fatal: %s\n", buf);
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_FATAL, "AshfordsHalberd", "fatal: %s", buf);
    if (g_window) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ashford's Halberd", buf, g_window);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ashford's Halberd", buf, g_window);
#endif
    exit(1);
}

void host_quit(void)
{
    SDL_Event q = { .type = SDL_EVENT_QUIT };
    SDL_PushEvent(&q);
}

void host_pump_events(void)
{
    SDL_Event ev;
    bind_test_frame();
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_GAMEPAD_ADDED || ev.type == SDL_EVENT_GAMEPAD_REMOVED) { extern void pad_devices_changed(void); pad_devices_changed(); }
        if (bind_event(&ev)) continue;   /* a "press a key" of the settings */
        if (overlay_event(&ev)) continue;
        bind_latch_event(&ev);
        if ((ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) || ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            SDL_AddAtomicInt(&presses, 1);
        tps_input_event(&ev);
        menu_mouse_event(&ev);
        switch (ev.type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            host_enter_background();
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            host_enter_foreground();
            break;
        case SDL_EVENT_RENDER_DEVICE_RESET:
            if (g_window) {
                overlay_context_lost(0);
                gs_renderer_context_lost(0);
                if (g_gl) { SDL_GL_DestroyContext(g_gl); g_gl = NULL; }
                g_app_suspended = 1;
                host_enter_foreground();
            }
            break;
        case SDL_EVENT_QUIT:
            config_save();
            exit(0);
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:   /* moved to another monitor: its modes for Fullscreen resolution */
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:   /* the desktop resized: a borderless window covers it again */
            settings_display_modes();
            break;
        case SDL_EVENT_KEY_DOWN:
            if (bind_hotkey(&ev, B_FMV_SKIP)) SDL_SetAtomicInt(&movie_skip, 1);
            if (bind_hotkey(&ev, B_FULLSCREEN)) {
                g_config.fullscreen = !g_config.fullscreen;   /* of the kind chosen last (Display mode) */
                settings_apply_display();
                settings_save_later();
            }
            if (bind_hotkey(&ev, B_TPS_TOGGLE)) {
                g_tps.on = !g_tps.on;
                settings_save_later();
            }
            if (bind_hotkey(&ev, B_FPS_TOGGLE)) {
                g_tps.on = g_tps.on == 2 ? 0 : 2;
                settings_save_later();
            }
            if (bind_hotkey(&ev, B_SPOILER)) spoiler_key(1);
            else if (ev.key.scancode == SDL_SCANCODE_ESCAPE && !ev.key.repeat) spoiler_key(2);
            else if (ev.key.scancode == SDL_SCANCODE_PAGEUP) spoiler_key(3);
            else if (ev.key.scancode == SDL_SCANCODE_PAGEDOWN) spoiler_key(4);
            else if (ev.key.scancode == SDL_SCANCODE_HOME) spoiler_key(5);
            else if (ev.key.scancode == SDL_SCANCODE_END) spoiler_key(6);
            break;
        }
    }
    settings_poll();
    if (__atomic_exchange_n(&g_import_reload, 0, __ATOMIC_ACQ_REL)) { import_registry_load(); lang_scan(); }
}

#ifndef _WIN32
#if defined(__ANDROID__)
struct watchdog_bt_ctx { void* frames[64]; int n; };
static _Unwind_Reason_Code watchdog_bt_trace(struct _Unwind_Context* uc, void* ud)
{
    struct watchdog_bt_ctx* c = ud;
    if (c->n < 64) c->frames[c->n++] = (void*)_Unwind_GetIP(uc);
    return c->n >= 64 ? _URC_END_OF_STACK : _URC_NO_REASON;
}
static void watchdog_write_all(int fd, const char* p, size_t n)
{
    while (n) { ssize_t k = write(fd, p, n); if (k <= 0) return; p += k; n -= (size_t)k; }
}
static char* watchdog_put_u(char* p, unsigned long v, unsigned base)
{
    char t[24];
    int n = 0;
    do t[n++] = "0123456789abcdef"[v % base]; while (v /= base);
    while (n) *p++ = t[--n];
    return p;
}
#endif
/* debugging aid: RECVX_WATCHDOG=<seconds> prints a backtrace and exits (headless runs) */
static void watchdog(int sig)
{
#if defined(__ANDROID__)
    struct watchdog_bt_ctx ctx = { .n = 0 };
    _Unwind_Backtrace(watchdog_bt_trace, &ctx);
    (void)sig;
    watchdog_write_all(2, "watchdog: backtrace\n", 20);
    for (int i = 0; i < ctx.n; i++) {
        char line[64], *p = line;
        *p++ = '#';
        p = watchdog_put_u(p, (unsigned long)i, 10);
        *p++ = ' '; *p++ = '0'; *p++ = 'x';
        p = watchdog_put_u(p, (unsigned long)(uintptr_t)ctx.frames[i], 16);
        *p++ = '\n';
        watchdog_write_all(2, line, (size_t)(p - line));
    }
#else
    void* frames[64];
    int n = backtrace(frames, 64);
    (void)sig;
    fprintf(stderr, "watchdog: backtrace\n");
    backtrace_symbols_fd(frames, n, 2);
#endif
    if (getenv("RECVX_PROFILE")) exit(3);   /* run atexit: profile dump */
    _exit(3);
}
#endif

static void usage(void)
{
    fprintf(stderr,
        "Ashford's Halberd " ASHFORD_VERSION " Beta / campaign engine " RECVX_VERSION "\n"
        "usage: AshfordsHalberd [options] <game disc image (.iso)>\n"
        "  --skip-launcher    start the campaign directly without the Ashford launcher\n"
        "  --save-dir <dir>   settings and saves directory (default: per-user AshfordsHalberd/Campaign)\n"
        "                     Mods and logs are resolved beside the program.\n"
        "  --size WxH         window size (default 1280x960)\n"
        "  --fullscreen       start in fullscreen (borderless, or exclusive if chosen in the settings)\n"
        "  --borderless / --exclusive / --windowed   start in that display mode\n"
        "  --enhanced         display-rate frames (60+ FPS) blended from the 30 FPS game (default)\n"
        "  --vanilla          original 30 FPS presentation\n"
        "  --skip-doors       room transitions (doors, stairs, ladders) without their animation (default;\n"
        "                     skip_doors in recvx.ini: 0 played, 1 instant, 2 played until a key is pressed)\n"
        "  --skip-intro       boot straight into the title menu (default on)\n"
        "  --hd-textures DIR  PCSX2 HD texture pack folder (default mods/textures)\n"
        "  --third-person     over-the-shoulder camera, mouse/keyboard controls (toggle: F5)\n"
        "  --first-person     first-person camera (toggle: F12 or L3+R3)\n"
        "  --quiet\n"
        "  --version          print the version and exit\n"
        "  --log[=file]       copy all output to a log file (default recvx.log), crash backtraces included\n"
        "  --profile[=file]   per-frame timing CSV (default recvx_profile.csv): logic, vu0, vu1, gs,\n"
        "                     interp, present, wait; also shown live in the F1 overlay\n"
        "  --profile-samples=file  sampling CPU profiler (summarise with pc/tools/profile.py)\n"
        "F1 opens the settings overlay; settings are saved in <save dir>/recvx.ini (options set there,\n"
        "e.g. skip_doors = 0 or skip_intro = 0, override the defaults).\n"
        "The image can also be given with the RECVX_ISO environment variable. Without one, a file\n"
        "selector asks for it on the first start and recvx.ini remembers it (key iso).\n");
}

static char log_path[1024], profile_path[1024];
static int iso_given;   /* image from the command line / RECVX_ISO: no file selector */
static int skip_launcher;
static int save_given;  /* --save-dir */
static char start_dir[1024];   /* where the program was started: relative image paths are taken from here */
static char default_save_dir[1024];
static char disc_error[1024];

static int open_game_image(void)
{
    if (!ashford_validate_disc(g_config.iso_path, disc_error, sizeof disc_error)) return 0;
    if (cdvd_open_image(g_config.iso_path)) return 1;
    SDL_snprintf(disc_error, sizeof disc_error, "Cannot open disc image: %s", g_config.iso_path);
    return 0;
}

static void user_data_dir(void)
{
    char* base = SDL_GetPrefPath("", "AshfordsHalberd");
    if (!base) host_fatal("Cannot locate the user-data directory: %s", SDL_GetError());
    if (SDL_snprintf(default_save_dir, sizeof default_save_dir, "%sCampaign", base) >= (int)sizeof default_save_dir)
        host_fatal("User-data directory path is too long");
    SDL_free(base);
}

static void iso_from(const char* s)
{
    int absolute = s[0] == '/' || s[0] == '\\' || (s[0] && s[1] == ':');
    if (absolute || !start_dir[0]) SDL_strlcpy(g_config.iso_path, s, sizeof(g_config.iso_path));
    else SDL_snprintf(g_config.iso_path, sizeof(g_config.iso_path), "%s%s", start_dir, s);
    iso_given = 1;
}

/* Started from somewhere else (a file manager, a shortcut, another directory): the program's own
   folder holds save/, mods/ and the logs, so the release binaries need no start script. A save
   folder in the current directory or an explicit --save-dir keeps the current directory (several
   instances with their own folders, test setups). */
static void home_dir(void)
{
    const char* base;
    if (save_given || SDL_GetPathInfo("save", NULL)) return;
    if (!(base = SDL_GetBasePath())) return;
    pc_chdir(base);
}

static void parse_args(int argc, char** argv)
{
    const char* env = SDL_getenv("RECVX_ISO");   /* UTF-8 on Windows too */
    if (env) iso_from(env);
    SDL_strlcpy(g_config.save_dir, default_save_dir, sizeof(g_config.save_dir));
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) { SDL_strlcpy(g_config.save_dir, argv[++i], sizeof(g_config.save_dir)); save_given = 1; }
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &g_config.window_w, &g_config.window_h);
        else if (!strcmp(argv[i], "--fullscreen")) g_config.fullscreen = 1;   /* the kind of recvx.ini (borderless by default) */
        else if (!strcmp(argv[i], "--borderless")) { g_config.fullscreen = 1; g_config.fullscreen_exclusive = 0; }
        else if (!strcmp(argv[i], "--exclusive")) { g_config.fullscreen = 1; g_config.fullscreen_exclusive = 1; }
        else if (!strcmp(argv[i], "--windowed")) g_config.fullscreen = 0;
        else if (!strcmp(argv[i], "--enhanced")) g_config.interpolate = 1;
        else if (!strcmp(argv[i], "--vanilla")) g_config.interpolate = 0;
        else if (!strcmp(argv[i], "--skip-doors")) g_config.skip_doors = 1;
        else if (!strcmp(argv[i], "--skip-intro")) g_config.skip_intro = 1;
        else if (!strcmp(argv[i], "--hd-textures") && i + 1 < argc) { SDL_strlcpy(g_config.hd_textures_dir, argv[++i], sizeof(g_config.hd_textures_dir)); }
        else if (!strcmp(argv[i], "--third-person")) g_tps.on = 1;
        else if (!strcmp(argv[i], "--first-person")) g_tps.on = 2;
        else if (!strcmp(argv[i], "--quiet")) g_config.log_level = 0;
        else if (!strcmp(argv[i], "--skip-launcher")) skip_launcher = 1;
        else if (!strncmp(argv[i], "--log", 5) && (argv[i][5] == 0 || argv[i][5] == '='))
            SDL_strlcpy(log_path, argv[i][5] ? argv[i] + 6 : "recvx.log", sizeof(log_path));
        else if (!strncmp(argv[i], "--profile-samples=", 18)) SDL_setenv_unsafe("RECVX_PROFILE", argv[i] + 18, 1);
        else if (!strncmp(argv[i], "--profile", 9) && (argv[i][9] == 0 || argv[i][9] == '='))
            SDL_strlcpy(profile_path, argv[i][9] ? argv[i] + 10 : "recvx_profile.csv", sizeof(profile_path));
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); exit(0); }
        else if (!strcmp(argv[i], "--version")) { printf("Ashford's Halberd " ASHFORD_VERSION " Beta / campaign engine " RECVX_VERSION " (recvx-enhanced-port)\n"); exit(0); }
        else if (argv[i][0] != '-') iso_from(argv[i]);
        else { usage(); exit(1); }
    }
}

/* First start (or the remembered image is gone): ask for the disc image, remember it in recvx.ini. */
static void SDLCALL iso_picked(void* done, const char* const* files, int filter)
{
    (void)filter;
    if (!files) host_log("file selector: %s\n", SDL_GetError());
    else if (files[0]) SDL_strlcpy(g_config.iso_path, files[0], sizeof(g_config.iso_path));
    SDL_SetAtomicInt(done, 1);
}

static void pick_iso(void)
{
    static const SDL_DialogFileFilter filters[] = { { "PS2 disc image (*.iso)", "iso" }, { "All files", "*" } };
    static SDL_AtomicInt done;
    SDL_PropertiesID props = SDL_CreateProperties();
    g_config.iso_path[0] = 0;
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, (void*)filters);
    SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, SDL_arraysize(filters));
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Select the Resident Evil Code: Veronica X disc image (SLUS-20184)");
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, iso_picked, &done, props);
    SDL_DestroyProperties(props);
    while (!SDL_GetAtomicInt(&done)) { SDL_Event ev; SDL_WaitEventTimeout(&ev, 50); }
}

/* The game (the decomp's main loop, VU, GIF recording) runs on its own thread; the main
   thread renders (host/render.c). */
static int SDLCALL game_thread(void* ud)
{
    char* game_argv[] = { "cdrom0:\\SLUS_201.84;1", NULL };
    (void)ud;
    { extern void crash_thread_init(void); crash_thread_init(); }
    prof_thread(PROF_LOGIC);
    { extern void profile_thread(char); profile_thread('g'); }   /* RECVX_PROFILE */
    exit(recvx_game_main(1, game_argv));
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* the game's printf diagnostics */
#if defined(__ANDROID__)
    /* Reserve [0x30000000, 0x50000000) + DMA pool, load librecvx_game.so at
       0x30000000, and resolve all game bridge symbols before touching game state. */
    ashford_window_init();
#endif
    argv = pc_utf8_argv(argc, argv);   /* paths are UTF-8 everywhere (fileio.h) */
    user_data_dir();
    { char* cwd = SDL_GetCurrentDirectory(); if (cwd) { SDL_strlcpy(start_dir, cwd, sizeof(start_dir)); SDL_free(cwd); } }
    parse_args(argc, argv);
    home_dir();
    { extern void crash_init(const char*); crash_init(ASHFORD_VERSION); }   /* AshfordsHalberd-crash.log, with or without --log */
    if (log_path[0]) { extern void log_start(const char*); log_start(log_path); }
    host_log("Ashford's Halberd " ASHFORD_VERSION " Beta / campaign backend " RECVX_VERSION "\n");
    host_log("Adapted from recvx-enhanced-port; Copyright (c) 2026 aikatore and contributors, MIT\n");
    if (profile_path[0]) { extern void prof_open_csv(const char*); prof_open_csv(profile_path); }
#ifndef _WIN32
    if (getenv("RECVX_WATCHDOG")) { signal(SIGALRM, watchdog); alarm((unsigned)atoi(getenv("RECVX_WATCHDOG"))); }
    { extern void profile_start(void); profile_start(); }
#endif
    config_load();
    parse_args(argc, argv);   /* command line overrides the saved settings */
    biorand_scan();
    { extern void pc_rifle_stack(int); pc_rifle_stack(g_config.rifle_stack); }
    { extern void variants_load(void); extern void enemy_rates_load(void); variants_load(); enemy_rates_load(); }   /* mods/enemies.ini: custom enemies, then "Custom"'s rates */
    ps2_memory_init();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
        host_fatal("SDL_Init: %s", SDL_GetError());
    bind_apply_source();   /* Gamepad only: gamepads also in the background */

    if (!skip_launcher) {
        char remembered_iso[sizeof g_config.iso_path];
        SDL_strlcpy(remembered_iso, g_config.iso_path, sizeof remembered_iso);
        for (;;) {
            if (!ashford_launcher(g_config.iso_path, sizeof(g_config.iso_path))) {
                SDL_strlcpy(g_config.iso_path, remembered_iso, sizeof g_config.iso_path);
                config_save();
                SDL_Quit();
                return 0;
            }
            if (open_game_image()) break;
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ashford's Halberd - Disc image", disc_error, NULL);
        }
        config_save();
    } else if (!iso_given && (!g_config.iso_path[0] || !open_game_image())) {
        pick_iso();
        if (!g_config.iso_path[0]) { SDL_Quit(); return 0; }   /* picker cancellation is a normal exit */
        if (!open_game_image()) host_fatal("%s", disc_error);
        config_save();
    } else {
        if (!g_config.iso_path[0]) { usage(); host_fatal("no game disc image given"); }
        if (!open_game_image()) host_fatal("%s", disc_error);
    }
    SDL_strlcpy(g_lang_us_iso, g_config.iso_path, sizeof(g_lang_us_iso));   /* language import reads US text */
    lang_scan();
    tr_init();

    host_apply_gl_attributes();
    g_window = SDL_CreateWindow("Ashford's Halberd", g_config.window_w, g_config.window_h,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!g_window) host_fatal("SDL_CreateWindow: %s", SDL_GetError());
    {
        char icon_path[2048];
        SDL_snprintf(icon_path, sizeof icon_path, "%sassets/branding/ashford-icon.bmp", SDL_GetBasePath());
        SDL_Surface* icon = SDL_LoadBMP(icon_path);
        if (icon) { SDL_SetWindowIcon(g_window, icon); SDL_DestroySurface(icon); }
    }
    settings_display_modes();
    settings_apply_display();   /* hidden: fullscreen (and its mode) are taken when it is shown */
    SDL_ShowWindow(g_window);
    g_gl = SDL_GL_CreateContext(g_window);
    if (!g_gl) host_fatal("SDL_GL_CreateContext: %s", SDL_GetError());
    SDL_GL_SetSwapInterval(g_config.vsync ? 1 : 0);
    gs_renderer_init();
    overlay_init(g_window, g_gl);
    audio_init();

    stream_init();
    if (!SDL_CreateThread(game_thread, "game", NULL)) host_fatal("cannot start the game thread: %s", SDL_GetError());
    render_loop();   /* the main thread renders and presents; never returns */
    return 0;
}
