#ifndef H_SERIALIZATION
#define H_SERIALIZATION

#include "typedefs.h"
typedef struct {
  size_t cur_offset;
  size_t len;
  uint8 *bytes;
} BinaryWriter;

bool32 write_u8(BinaryWriter *writer, u8 *arr, u32 len);

bool32 write_u32(BinaryWriter *writer, u32 v);

bool32 write_u64(BinaryWriter *writer, u64 v);

bool32 write_i32(BinaryWriter *writer, i32 v);

bool32 write_f32(BinaryWriter *writer, f32 v);

bool32 write_u32_array(BinaryWriter *writer, u32 *arr, u32 len);

bool32 write_f32_array(BinaryWriter *writer, f32 *arr, u32 len);

typedef struct {
  size_t len;
  size_t cur_offset;
  const uint8 *bytes;
} BinaryReader;

bool32 read_u8_array(BinaryReader *reader, u8 *arr, u32 len);

bool32 read_u32(BinaryReader *reader, u32 *v);

bool32 read_u64(BinaryReader *reader, u64 *v);

bool32 read_i32(BinaryReader *reader, i32 *v);

bool32 read_f32(BinaryReader *reader, f32 *v);

bool32 read_f32_array(BinaryReader *reader, f32 *arr, u32 len);

bool32 read_u32_array(BinaryReader *reader, u32 *arr, u32 len);

#endif
