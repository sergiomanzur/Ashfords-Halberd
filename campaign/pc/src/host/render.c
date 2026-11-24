/*
 * Render thread (the main thread: it owns the window, OpenGL and the event pump).
 *
 * Game frames arrive from the game thread as recorded GS streams (gs/stream.c). Each is
 * executed once (the real image) and then presented in step with the display: with
 * interpolation, every display refresh during the following game-frame period shows the
 * frame replayed at t = elapsed / period between the previous and the new frame, so any
 * refresh rate gets evenly spaced in-between images; game logic stays at its original rate.
 */
#include "../ps2_platform.h"
#include "../gs/gs.h"
#include "../gs/stream.h"
#include "prof.h"
#include "pacing.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

uint64_t gs_frame_count(void);

extern SDL_Window* g_window;

static double display_hz(void)
{
    static double test_hz = -1;   /* RECVX_DISPLAY_HZ (test aid): the refresh rate to assume, e.g. 1000 with vsync off */
    if (test_hz < 0) test_hz = SDL_getenv("RECVX_DISPLAY_HZ") ? SDL_atof(SDL_getenv("RECVX_DISPLAY_HZ")) : 0;
    if (test_hz > 1) return test_hz;
    const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(g_window));
    return m && m->refresh_rate > 1.0f ? m->refresh_rate : 60.0;
}

/* images per second: the display's refresh, or the "Frame rate limit" below it */
static double present_hz(void)
{
    static const int fps_limits[] = { 0, 30, 60, 90, 120, 144 };   /* settings.c names */
    double hz = display_hz();
    int lim = g_config.fps_limit > 0 && g_config.fps_limit < 6 ? fps_limits[g_config.fps_limit] : 0;
    return lim && lim < hz ? lim : hz;
}

double render_present_hz(void) { return present_hz(); }   /* (gs_gl.c drs_update) */

void render_loop(void)
{
    Frame *prev = NULL, *cur = NULL;
    uint64_t arrived = 0, pace_until = 0, last_present = 0;
    double pace_gap = SDL_getenv("RECVX_PACE_GAP") ? SDL_atof(SDL_getenv("RECVX_PACE_GAP")) : 1.5;
    double swap_avg = 0.0;   /* how long the buffer swap blocks, smoothed */
    double period = 1001.0 / 30000.0 * 1e9;   /* game frame period, measured below */
    int interp = 0, shown = 0;   /* refreshes presented since the frame arrived */
    int done = 0;   /* cur's own image (t = 1) is up */
    int fallback = !SDL_getenv("RECVX_PACE_FALLBACK") || SDL_atoi(SDL_getenv("RECVX_PACE_FALLBACK"));
    unsigned skip_q = 0, skip_gpu = 0, skip_fb = 0, n_inb = 0, n_exe = 0;
    uint64_t gpu_wait = 0, executed = 0;
    int check_replay = SDL_getenv("RECVX_REPLAY_CHECK") && SDL_atoi(SDL_getenv("RECVX_REPLAY_CHECK"));
    float fixed_t = SDL_getenv("RECVX_INTERP_T") ? (float)SDL_atof(SDL_getenv("RECVX_INTERP_T")) : 0.0f;   /* test aid: first refresh shows t */
    FILE* trace = SDL_getenv("RECVX_TRACE_PRESENT") ? fopen(SDL_getenv("RECVX_TRACE_PRESENT"), "w") : NULL;   /* test aid */
    if (trace) {
        int interval = -2;
        SDL_GL_GetSwapInterval(&interval);
        fprintf(trace, "# display_hz=%.3f requested_vsync=%d driver_swap_interval=%d\n", display_hz(), g_config.vsync, interval);
        fprintf(trace, "# game_frame interpolation start_us swap_block_us completion_us executed\n");
    }
    prof_thread(PROF_RIDLE);
    for (;;) {
        Frame* f;
        uint64_t now;
        host_pump_events();
        if (stream_snapshot_service() == 2) {
            stream_release(prev); stream_release(cur); prev=cur=NULL;
            arrived=pace_until=last_present=0; swap_avg=0; period=1001.0/30000.0*1e9; interp=shown=done=0;
            continue;
        }
        /* nothing to show yet: wait for the game; otherwise take what has arrived, waiting for
           it until the next refresh is due (a frame arriving meanwhile is executed at once) */
        now = SDL_GetTicksNS();
        f = stream_pop(!cur ? 20 : pace_until > now ? (int)((pace_until - now) / 1000000) : 0);   /* the rest: SDL_DelayPrecise */
        if (f) {
            prof_push(PROF_EXEC);
            gs_frame_begin();
            stream_exec(f);
            prof_pop();
            if (stream_is_partial(f)) { stream_release(f); continue; }   /* download sync point */
            now = SDL_GetTicksNS();
            if (arrived) {
                double d = (double)(now - arrived);
                if (d > 10e6 && d < 100e6) period += 0.1 * (d - period);
            }
            arrived = now;
            shown = 0;
            done = 0;
            executed++;
            prof_push(PROF_INTERP);
            interp = g_config.interpolate && stream_match(cur, f);
            prof_pop();
            gs_frame_executed(interp);
            if (interp && check_replay) gs_replay_check(f);
            stream_release(prev);
            prev = cur;
            cur = f;
        }
        if (!cur) continue;
        now = SDL_GetTicksNS();
        if (pace_until > now) {   /* the refresh is not due yet: a frame that arrived is executed, its
                                     image waits for the refresh like any other (shown at once, it cut
                                     the previous image short); woken early (ms timeout): the rest */
            SDL_DelayPrecise(pace_until - now);
            if (!f) continue;
            now = SDL_GetTicksNS();
        }
        {
            /* With a display rate that is a whole multiple of the game's (60/120/240 Hz), step by
               refreshes: t = 1/n, 2/n, ... 1, evenly spaced however the game thread's frame
               delivery jitters against vsync. Otherwise by time since arrival. */
            double n = period * 1e-9 * present_hz();
            uint64_t tick = (uint64_t)(1e9 / present_hz());
            int step_interpolation = g_config.vsync && n > 1.5 && fabs(n - floor(n + 0.5)) < 0.1;
            float t;
            uint64_t swap;
            if (!interp) t = 1.0f;
            else if (fixed_t > 0.0f) t = shown ? 1.0f : fixed_t;
            else if (step_interpolation) t = (float)((shown + 1) / floor(n + 0.5));
            else t = (float)((double)(now - arrived) / period);
            /* When an in-between image cannot be made, the frame's own image (t = 1: a copy, no
               replay, hardly any GPU work) is the fallback, once per frame; after it nothing older
               of the frame is shown. (RECVX_INTERP_T runs never come here.) */
            if (t < 1.0f && fixed_t <= 0.0f) {
                int ready = 0;
                if (stream_queued()) {
                    if (done || now - last_present < (uint64_t)(period * pace_gap)) { skip_q++; continue; }
                } else if (done) {   /* its own image is up: nothing older of it any more */
                    shown++;
                    pace_until = present_next_deadline(pace_until, now, tick);
                    continue;
                } else {
                    uint64_t w0 = trace ? SDL_GetTicksNS() : 0;
                    ready = gs_gpu_ready(tick / 8 * 7);
                    if (trace) gpu_wait += SDL_GetTicksNS() - w0;
                    if (!ready && !(fallback && !step_interpolation && (double)(SDL_GetTicksNS() - arrived) + 1.25 * tick > period)) {
                        skip_gpu++;
                        shown++;
                        pace_until = present_next_deadline(pace_until, SDL_GetTicksNS(), tick);
                        continue;
                    }
                }
                if (!ready) { skip_fb++; t = 1.0f; }
            }
            now = SDL_GetTicksNS();   /* the GPU fence may have consumed most of a refresh */
            if (interp && fixed_t <= 0.0f && !step_interpolation && t < 1.0f) t = (float)((double)(now - arrived) / period);
            swap = gs_present(cur, t);
            last_present = now;
            shown++;
            if (t >= 1.0f) done = 1;
            if (t < 1.0f) n_inb++; else n_exe++;
            if (trace && (n_inb + n_exe) % 300 == 0)
                fprintf(trace, "# in-between %u executed %u skipped: queued %u gpu %u, own image instead %u, gpu wait %.1f ms\n", n_inb, n_exe, skip_q, skip_gpu, skip_fb, gpu_wait / 1e6);
            if (trace) fprintf(trace, "%llu %.3f %llu %llu %llu %llu\n", (unsigned long long)gs_frame_count(), t, (unsigned long long)(now / 1000),
                               (unsigned long long)(swap / 1000), (unsigned long long)(SDL_GetTicksNS() / 1000), (unsigned long long)executed);
            /* pace to the display refresh instead of spinning: without vsync, and with vsync when
               the swap does not wait (the offscreen driver, some compositors): presenting as fast
               as the GPU allows starved the frames themselves (RM_1310 headless: 197 -> 37 ms a
               game frame in vanilla mode). Whether the swap waits is its own duration: one that
               waits blocks for part of a refresh on average. Presents start one refresh apart
               (the wait is the stream_pop above), so every image, in-between or not, is shown
               for one refresh. */
            {   /* on a fixed grid (a late present does not shift the following ones) */
                swap_avg += ((double)swap - swap_avg) / 8;
                if (present_swap_paces(g_config.vsync, swap_avg, swap, tick, present_hz() != display_hz())) pace_until = 0;
                else pace_until = present_next_deadline(pace_until, now, tick);
            }
        }
    }
}
