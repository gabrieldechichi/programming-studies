#ifndef H_BLOB_ASSET
#define H_BLOB_ASSET

#include "lib/assert.h"
#include "lib/hash.h"
#include "lib/typedefs.h"

typedef struct {
  u32 offset;    // Offset in bytes from parent base pointer
  u32 size;      // Total size in bytes of the blob
  u32 type_size; // Size of each element (for validation and length calculation)
  u32 typehash;  // Hash of type name + size for validation
} BlobPtr;

#define BlobArray(type) BlobPtr

#define TYPE_HASH(T) (fnv1a_hash(#T) ^ (u32)sizeof(T))

// Get pointer to asset data with type validation
force_inline void *_blobptr_get(void *parent, BlobPtr ptr,
                                  size_t expected_type_size,
                                  u32 expected_typehash) {
  // Validate type_size matches (catches basic size mismatches)
  assert_msg(expected_type_size == ptr.type_size,
             "BlobPtr type_size mismatch. Expected %, Got %", FMT_UINT(expected_type_size), FMT_UINT(ptr.type_size));

  // Validate typehash matches (catches type name + size changes)
  assert_msg(expected_typehash == ptr.typehash, "BlobPtr typehash mismatch. Expected %, Got %", FMT_UINT(expected_typehash), FMT_UINT(ptr.typehash));

  return (void *)((u8 *)parent + ptr.offset);
}

// Get number of elements in asset array
force_inline u32 blobptr_len(BlobPtr ptr) {
  // Validate size is aligned to type_size
  assert_msg(ptr.size % ptr.type_size == 0,
             "BlobPtr size not aligned to type_size");
  return ptr.size / ptr.type_size;
}

#define blob_array_get(type, parent, ptr)                                        \
  ((type *)_blobptr_get(parent, ptr, sizeof(type), TYPE_HASH(type)))

#define blob_array_get_void(parent, ptr) ((void *)((u8 *)parent + ptr.offset))

#define ASSET_VERSION 3

typedef struct {
    u32 len;
    u32 offset;
} StringBlob;

typedef struct {
    u32 version;
    u64 asset_size;
    u64 asset_type_hash;
    u32 dependency_count;
} BlobAssetHeader;

force_inline char *string_blob_get(void *base, StringBlob str) {
    return (char *)((u8 *)base + str.offset);
}

#endif
