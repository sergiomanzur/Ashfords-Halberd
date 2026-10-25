#include "quicksave.h"
#include "../ps2_platform.h"
#include <SDL3/SDL.h>
#include <stdio.h>
static SDL_SpinLock notice_lock;
static int notice_id;
static Uint64 until;
static const char* const notices[] = {
    "", "Quick action blocked: wait for normal player control.",
    "Quick action blocked: an active enemy is in this room.",
    "Quick action blocked: a countdown is active.",
    "No quicksave found.", "Quicksave is invalid; current game kept.",
    "Quicksave failed; the previous quicksave was kept.", "Quicksave saved.",
    "Loading quicksave...", "Quicksave restored.",
    "Quick action blocked: unsupported player or room state.",
    "Press quicksave or quickload separately.", "Cannot read quicksave; current game kept.",
    "Quickload failed; current game kept.",
    "Quicksave belongs to another game build; current game kept.",
    "Room snapshots are unavailable in this build."
};
void quicksave_notice_set(int message)
{
    SDL_LockSpinlock(&notice_lock);
    notice_id = message > 0 && message < (int)(sizeof(notices) / sizeof(notices[0])) ? message : 0;
    until = SDL_GetTicks() + 4500;
    SDL_UnlockSpinlock(&notice_lock);
}
const char* quicksave_notice(void)
{
    const char* result = NULL;
    SDL_LockSpinlock(&notice_lock);
    if (notice_id && SDL_GetTicks() < until) result = notices[notice_id];
    SDL_UnlockSpinlock(&notice_lock);
    return result;
}
const char* quicksave_path(void)
{
    static char path[1200];
    if (!path[0]) snprintf(path, sizeof(path), "%s/quicksave.ahs", g_config.save_dir);
    return path;
}
