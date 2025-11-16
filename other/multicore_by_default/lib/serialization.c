#include "serialization.h"
#include "assert.h"
#include "fmt.h"
#include "memory.h"
#include <string.h>

bool32 write_u8(BinaryWriter *writer, u8 *arr, u32 len) {
  size_t end_offset = writer->cur_offset + sizeof(u8) * len;
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], arr, len * sizeof(u8));
  writer->cur_offset = end_offset;
  return true;
}

bool32 write_u32(BinaryWriter *writer, u32 v) {
  size_t end_offset = writer->cur_offset + sizeof(u32);
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], &v, sizeof(u32));
  writer->cur_offset = end_offset;

  return true;
}

bool32 write_u64(BinaryWriter *writer, u64 v) {
  size_t end_offset = writer->cur_offset + sizeof(u64);
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], &v, sizeof(u64));
  writer->cur_offset = end_offset;

  return true;
}

bool32 write_i32(BinaryWriter *writer, i32 v) {
  size_t end_offset = writer->cur_offset + sizeof(i32);
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], &v, sizeof(i32));
  writer->cur_offset = end_offset;

  return true;
}

bool32 write_f32(BinaryWriter *writer, f32 v) {
  size_t end_offset = writer->cur_offset + sizeof(f32);
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], &v, sizeof(f32));
  writer->cur_offset = end_offset;

  return true;
}

bool32 write_u32_array(BinaryWriter *writer, u32 *arr, u32 len) {
  if (len == 0) {
    return true;
  }
  size_t end_offset = writer->cur_offset + sizeof(u32) * len;
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], arr, len * sizeof(u32));
  writer->cur_offset = end_offset;
  return true;
}

bool32 write_f32_array(BinaryWriter *writer, f32 *arr, u32 len) {
  size_t end_offset = writer->cur_offset + sizeof(f32) * len;
  bool32 within_range = end_offset <= writer->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(&writer->bytes[writer->cur_offset], arr, len * sizeof(f32));
  writer->cur_offset = end_offset;
  return true;
}

bool32 read_u8_array(BinaryReader *reader, u8 *arr, u32 len) {
  size_t end_offset = reader->cur_offset + sizeof(u8) * len;
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(arr, &reader->bytes[reader->cur_offset], len * sizeof(u8));
  reader->cur_offset = end_offset;
  return true;
}

bool32 read_u32(BinaryReader *reader, u32 *v) {
  size_t end_offset = reader->cur_offset + sizeof(u32);
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(v, &reader->bytes[reader->cur_offset], sizeof(u32));

  reader->cur_offset = end_offset;

  return true;
}

bool32 read_u64(BinaryReader *reader, u64 *v) {
  size_t end_offset = reader->cur_offset + sizeof(u64);
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(v, &reader->bytes[reader->cur_offset], sizeof(u64));

  reader->cur_offset = end_offset;

  return true;
}

bool32 read_i32(BinaryReader *reader, i32 *v) {
  size_t end_offset = reader->cur_offset + sizeof(i32);
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(v, &reader->bytes[reader->cur_offset], sizeof(i32));

  reader->cur_offset = end_offset;

  return true;
}

bool32 read_f32(BinaryReader *reader, f32 *v) {
  size_t end_offset = reader->cur_offset + sizeof(f32);
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(v, &reader->bytes[reader->cur_offset], sizeof(f32));

  reader->cur_offset = end_offset;

  return true;
}

bool32 read_f32_array(BinaryReader *reader, f32 *arr, u32 len) {
  size_t end_offset = reader->cur_offset + sizeof(f32) * len;
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(arr, &reader->bytes[reader->cur_offset], len * sizeof(f32));
  reader->cur_offset = end_offset;
  return true;
}

bool32 read_u32_array(BinaryReader *reader, u32 *arr, u32 len) {
  size_t end_offset = reader->cur_offset + sizeof(u32) * len;
  bool32 within_range = end_offset <= reader->len;
  assert(within_range);
  if (!within_range) {
    return false;
  }

  memcpy_safe(arr, &reader->bytes[reader->cur_offset], len * sizeof(u32));
  reader->cur_offset = end_offset;
  return true;
}
