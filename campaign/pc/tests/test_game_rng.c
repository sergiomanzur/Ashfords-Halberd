#include "../src/host/game_rng.h"
#include <stdio.h>
#include <string.h>
int recvx_rand(void);
void recvx_srand(unsigned seed);
int main(void)
{
    GameRngState saved, fresh;
    int expected[128];
    game_rng_capture(&fresh);
    if (!game_rng_validate(&fresh) || fresh.seeded) return 1;
    recvx_srand(1);
    if (recvx_rand() != 1804289383) return 2;
    for (int i = 0; i < 75; ++i) recvx_rand();
    game_rng_capture(&saved);
    if (!game_rng_validate(&saved)) return 3;
    for (int i = 0; i < 128; ++i) expected[i] = recvx_rand();
    recvx_srand(42);
    game_rng_restore(&saved);
    for (int i = 0; i < 128; ++i) if (recvx_rand() != expected[i]) return 4;
    saved.front = 31;
    if (game_rng_validate(&saved)) return 5;
    game_rng_restore(&fresh);
    if (recvx_rand() != 1804289383) return 6;
    puts("game RNG snapshot continuation passed");
    return 0;
}
