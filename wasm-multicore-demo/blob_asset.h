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

#define BLOB_PTR_DEF(type) typedef BlobPtr BlobPtr_##type
#define BLOB_ARRAY_DEF(type) typedef BlobPtr BlobArray_##type

#define TYPE_HASH(T) (fnv1a_hash(#T) ^ (u32)sizeof(T))

// Get pointer to asset data with type validation
force_inline void *_assetptr_get(void *parent, BlobPtr ptr,
                                  size_t expected_type_size,
                                  u32 expected_typehash) {
  // Validate type_size matches (catches basic size mismatches)
  assert_msg(expected_type_size == ptr.type_size,
             "BlobPtr type_size mismatch");

  // Validate typehash matches (catches type name + size changes)
  assert_msg(expected_typehash == ptr.typehash, "BlobPtr typehash mismatch");

  return (void *)((u8 *)parent + ptr.offset);
}

// Get number of elements in asset array
force_inline u32 assetptr_len(BlobPtr ptr) {
  // Validate size is aligned to type_size
  assert_msg(ptr.size % ptr.type_size == 0,
             "BlobPtr size not aligned to type_size");
  return ptr.size / ptr.type_size;
}

#define assetptr_get(type, parent, ptr)                                        \
  ((type *)_assetptr_get(parent, ptr, sizeof(type), TYPE_HASH(type)))

#define ASSET_VERSION 3

typedef struct {
    u32 len;
    u32 offset;
} StringBlob;

typedef BlobPtr BlobArray;

typedef struct {
    u32 version;
    u64 asset_size;
    u64 asset_type_hash;
    u32 dependency_count;
} BlobAssetHeader;

force_inline void *blob_array_get(void *base, BlobArray arr) {
    return (u8 *)base + arr.offset;
}

force_inline char *string_blob_get(void *base, StringBlob str) {
    return (char *)((u8 *)base + str.offset);
}

#endif
