#include "random.h"

PCG32_State pcg32_new(u64 seed, u64 stream) {
  PCG32_State p = {0};
  pcg32_seed(&p, seed, stream);
  return p;
}

void pcg32_seed(PCG32_State *rng, u64 seed, u64 stream) {
  rng->state = 0;
  rng->stream = (stream << 1) | 1;
  pcg32_next(rng);
  rng->state += seed;
  pcg32_next(rng);
}

u32 pcg32_next(PCG32_State *rng) {
  u64 oldstate = rng->state;
  rng->state = oldstate * 6364136223846793005ULL + rng->stream;
  u32 xorshifted = cast(u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
  u32 rot = cast(u32)(oldstate >> 59u);
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

f32 pcg32_next_f32(PCG32_State *rng) {
  return cast(f32)(pcg32_next(rng) >> 8) * 0x1.0p-24f;
}

f32 pcg32_next_f32_range(PCG32_State *rng, f32 min, f32 max) {
  return min + pcg32_next_f32(rng) * (max - min);
}

u32 pcg32_next_u32_range(PCG32_State *rng, u32 min, u32 max) {
  u32 range = max - min;
  u32 threshold = (-range) % range;

  for (;;) {
    u32 r = pcg32_next(rng);
    if (r >= threshold) {
      return min + (r % range);
    }
  }
}

void xorshift32_seed(Xorshift32_State *rng, u32 seed) {
  rng->state = seed != 0 ? seed : 1;
}

u32 xorshift32_next(Xorshift32_State *rng) {
  rng->state ^= rng->state << 13;
  rng->state ^= rng->state >> 17;
  rng->state ^= rng->state << 5;
  return rng->state;
}

f32 xorshift32_next_f32(Xorshift32_State *rng) {
  return cast(f32)(xorshift32_next(rng) >> 8) * 0x1.0p-24f;
}

f32 xorshift32_next_f32_range(Xorshift32_State *rng, f32 min, f32 max) {
  return min + xorshift32_next_f32(rng) * (max - min);
}

u32 xorshift32_next_u32_range(Xorshift32_State *rng, u32 min, u32 max) {
  u32 range = max - min;
  u32 threshold = (-range) % range;

  for (;;) {
    u32 r = xorshift32_next(rng);
    if (r >= threshold) {
      return min + (r % range);
    }
  }
}