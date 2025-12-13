#ifndef H_HASH
#define H_HASH

#include "typedefs.h"
#include <math.h>

force_inline u32 fnv1a_hash(const char *bytes) {
  u32 hash = 0x811c9dc5;        // FNV offset basis

  while (*bytes) {
    hash ^= (u8)*bytes++;
    hash *= 0x01000193; // FNV prime
  }

  return hash;
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
