#pragma once
#include <stdint.h>
typedef struct { int32_t words[31]; uint32_t front, back, seeded; } GameRngState;
void game_rng_capture(GameRngState* out);
int game_rng_validate(const GameRngState* state);
void game_rng_restore(const GameRngState* state);
