#ifndef H_JSON_SERIALIZER
#define H_JSON_SERIALIZER

#include "memory.h"
#include "typedefs.h"

typedef struct {
  char *buffer;
  uint32 pos;
  uint32 capacity;
  Allocator *arena;
} JsonSerializer;

// Serializer initialization
JsonSerializer json_serializer_init(Allocator *arena, uint32 initial_capacity);
char *json_serializer_finalize(JsonSerializer *serializer);

// Primitive value serializers
void serialize_string_value(JsonSerializer *serializer, const char *value);
void serialize_string_value_len(JsonSerializer *serializer, const char *value,
                                uint32 len);
void serialize_number_value(JsonSerializer *serializer, double value);
void serialize_bool_value(JsonSerializer *serializer, bool32 value);
void serialize_null_value(JsonSerializer *serializer);

// Structural serialization helpers
void write_object_start(JsonSerializer *serializer);
void write_object_end(JsonSerializer *serializer);
void write_array_start(JsonSerializer *serializer);
void write_array_end(JsonSerializer *serializer);
void write_key(JsonSerializer *serializer, const char *key);
void write_comma(JsonSerializer *serializer);
void write_colon(JsonSerializer *serializer);

// Low-level write functions
void write_char(JsonSerializer *serializer, char c);
void write_string(JsonSerializer *serializer, const char *str);
void ensure_capacity(JsonSerializer *serializer, uint32 additional_bytes);

#endif