#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(void)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }
    SDL_version version;
    SDL_GetVersion(&version);
    printf("Native C and SDL %u.%u.%u OK; pointer width %u bits\n",
           version.major, version.minor, version.patch,
           (unsigned)(sizeof(void *) * 8));
    SDL_Quit();
    return 0;
}
