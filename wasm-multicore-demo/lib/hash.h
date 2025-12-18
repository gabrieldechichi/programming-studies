#ifndef H_HASH
#define H_HASH

#include "typedefs.h"
#include "memory.h"
#include <math.h>

force_inline u32 fnv1a_hash(const char *bytes) {
  u32 hash = 0x811c9dc5;
  while (*bytes) {
    hash ^= (u8)*bytes++;
    hash *= 0x01000193;
  }
  return hash;
}

#define WYHASH_CONDOM 1
#define WYHASH_LITTLE_ENDIAN 1

global const u64 wyp_[4] = {
    0xa0761d6478bd642full,
    0xe7037ed1a0b428dbull,
    0x8ebc6af09c88c6e3ull,
    0x589965cc75374cc3ull
};

force_inline void wymum_(u64 *A, u64 *B) {
    u64 ha = *A >> 32, hb = *B >> 32, la = (u32)*A, lb = (u32)*B, hi, lo;
    u64 rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb, t = rl + (rm0 << 32), c = t < rl;
    lo = t + (rm1 << 32);
    c += lo < t;
    hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
#if (WYHASH_CONDOM > 1)
    *A ^= lo;
    *B ^= hi;
#else
    *A = lo;
    *B = hi;
#endif
}

force_inline u64 wymix_(u64 A, u64 B) {
    wymum_(&A, &B);
    return A ^ B;
}

force_inline u64 wyr8_(const u8 *p) {
    u64 v;
    memcpy(&v, p, 8);
    return v;
}

force_inline u64 wyr4_(const u8 *p) {
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

force_inline u64 wyr3_(const u8 *p, size_t k) {
    return (((u64)p[0]) << 16) | (((u64)p[k >> 1]) << 8) | p[k - 1];
}

force_inline u64 wyhash(const void *key, size_t len, u64 seed, const u64 *secret) {
    const u8 *p = (const u8 *)key;
    seed ^= wymix_(seed ^ secret[0], secret[1]);
    u64 a, b;

    if (len <= 16) {
        if (len >= 4) {
            a = (wyr4_(p) << 32) | wyr4_(p + ((len >> 3) << 2));
            b = (wyr4_(p + len - 4) << 32) | wyr4_(p + len - 4 - ((len >> 3) << 2));
        } else if (len > 0) {
            a = wyr3_(p, len);
            b = 0;
        } else {
            a = b = 0;
        }
    } else {
        size_t i = len;
        if (i > 48) {
            u64 see1 = seed, see2 = seed;
            do {
                seed = wymix_(wyr8_(p) ^ secret[1], wyr8_(p + 8) ^ seed);
                see1 = wymix_(wyr8_(p + 16) ^ secret[2], wyr8_(p + 24) ^ see1);
                see2 = wymix_(wyr8_(p + 32) ^ secret[3], wyr8_(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed = wymix_(wyr8_(p) ^ secret[1], wyr8_(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = wyr8_(p + i - 16);
        b = wyr8_(p + i - 8);
    }

    a ^= secret[1];
    b ^= seed;
    wymum_(&a, &b);
    return wymix_(a ^ secret[0] ^ len, b ^ secret[1]);
}

force_inline u64 flecs_hash(const void *data, i32 length) {
    return wyhash(data, (size_t)length, 0, wyp_);
}

// Spatial hashing for 3D collision detection
// Uses prime multipliers to minimize hash collisions across cell coordinates
force_inline u32 spatial_hash_3i(i32 x, i32 y, i32 z) {
  return (u32)((x * 73856093) ^ (y * 19349663) ^ (z * 83492791));
}

force_inline u32 spatial_hash_3f(f32 px, f32 py, f32 pz, f32 cell_size) {
  i32 ix = (i32)floorf(px / cell_size);
  i32 iy = (i32)floorf(py / cell_size);
  i32 iz = (i32)floorf(pz / cell_size);
  return spatial_hash_3i(ix, iy, iz);
}

// Returns cell coordinates for a position
force_inline void spatial_cell_coords(f32 px, f32 py, f32 pz, f32 cell_size,
                                       i32 *out_x, i32 *out_y, i32 *out_z) {
  *out_x = (i32)floorf(px / cell_size);
  *out_y = (i32)floorf(py / cell_size);
  *out_z = (i32)floorf(pz / cell_size);
}

#endif
