#ifndef H_HASH
#define H_HASH

#include "typedefs.h"

force_inline u32 fnv1a_hash(const char *bytes) {
  u32 hash = 0x811c9dc5;        // FNV offset basis

  while (*bytes) {
    hash ^= (u8)*bytes++;
    hash *= 0x01000193; // FNV prime
  }

  return hash;
}

#endif
