#ifndef H_RANDOM
#define H_RANDOM

#include "typedefs.h"

typedef struct {
  u64 state;
  u64 stream;
} PCG32_State;

typedef struct {
  u32 state;
} Xorshift32_State;

PCG32_State pcg32_new(u64 seed, u64 stream);
void pcg32_seed(PCG32_State *rng, u64 seed, u64 stream);
u32 pcg32_next(PCG32_State *rng);
f32 pcg32_next_f32(PCG32_State *rng);
f32 pcg32_next_f32_range(PCG32_State *rng, f32 min, f32 max);
u32 pcg32_next_u32_range(PCG32_State *rng, u32 min, u32 max);

void xorshift32_seed(Xorshift32_State *rng, u32 seed);
u32 xorshift32_next(Xorshift32_State *rng);
f32 xorshift32_next_f32(Xorshift32_State *rng);
f32 xorshift32_next_f32_range(Xorshift32_State *rng, f32 min, f32 max);
u32 xorshift32_next_u32_range(Xorshift32_State *rng, u32 min, u32 max);

typedef struct {
    u32 state;
} UnityRandom;

UnityRandom unity_random_new(u32 seed);
u32 unity_random_next(UnityRandom *rng);
f32 unity_random_next_f32(UnityRandom *rng);

#endif