#include "json_serializer.h"
#include "common.h"
#include "lib/string.h"
#include <string.h>

// Serializer initialization
JsonSerializer json_serializer_init(Allocator *arena, uint32 initial_capacity) {
  JsonSerializer serializer = {0};
  serializer.capacity = initial_capacity;
  serializer.buffer = ALLOC_ARRAY(arena, char, initial_capacity);
  serializer.pos = 0;
  serializer.arena = arena;
  return serializer;
}

char *json_serializer_finalize(JsonSerializer *serializer) {
  // Null-terminate the buffer
  ensure_capacity(serializer, 1);
  serializer->buffer[serializer->pos] = '\0';
  return serializer->buffer;
}

// Low-level write functions
void ensure_capacity(JsonSerializer *serializer, uint32 additional_bytes) {
  uint32 needed = serializer->pos + additional_bytes;
  assert(needed < serializer->capacity);
}

void write_char(JsonSerializer *serializer, char c) {
  ensure_capacity(serializer, 1);
  serializer->buffer[serializer->pos++] = c;
}

void write_string(JsonSerializer *serializer, const char *str) {
  if (!str)
    return;
  uint32 len = str_len(str);
  ensure_capacity(serializer, len);
  memcpy(serializer->buffer + serializer->pos, str, len);
  serializer->pos += len;
}

// Primitive value serializers
void serialize_string_value(JsonSerializer *serializer, const char *value) {
  write_char(serializer, '"');

  if (value) {
    // Write string with proper JSON escaping
    for (const char *p = value; *p; p++) {
      char c = *p;
      if (c == '"') {
        write_string(serializer, "\\\"");
      } else if (c == '\\') {
        write_string(serializer, "\\\\");
      } else if (c == '\n') {
        write_string(serializer, "\\n");
      } else if (c == '\r') {
        write_string(serializer, "\\r");
      } else if (c == '\t') {
        write_string(serializer, "\\t");
      } else if (c == '\b') {
        write_string(serializer, "\\b");
      } else if (c == '\f') {
        write_string(serializer, "\\f");
      } else if (c < 32 || c == 127) {
        // Skip other control characters that could break JSON
        continue;
      } else {
        write_char(serializer, c);
      }
    }
  }

  write_char(serializer, '"');
}

void serialize_string_value_len(JsonSerializer *serializer, const char *value,
                                uint32 len) {
  write_char(serializer, '"');

  if (value && len > 0) {
    // Write string with proper JSON escaping, but only up to len characters
    for (uint32 i = 0; i < len; i++) {
      char c = value[i];
      if (c == '"') {
        write_string(serializer, "\\\"");
      } else if (c == '\\') {
        write_string(serializer, "\\\\");
      } else if (c == '\n') {
        write_string(serializer, "\\n");
      } else if (c == '\r') {
        write_string(serializer, "\\r");
      } else if (c == '\t') {
        write_string(serializer, "\\t");
      } else if (c == '\b') {
        write_string(serializer, "\\b");
      } else if (c == '\f') {
        write_string(serializer, "\\f");
      } else if (c < 32 || c == 127) {
        // Skip other control characters that could break JSON
        continue;
      } else {
        write_char(serializer, c);
      }
    }
  }

  write_char(serializer, '"');
}

void serialize_number_value(JsonSerializer *serializer, double value) {
  char temp[64];
  double_to_str(value, temp);
  write_string(serializer, temp);
}

void serialize_bool_value(JsonSerializer *serializer, bool32 value) {
  if (value) {
    write_string(serializer, "true");
  } else {
    write_string(serializer, "false");
  }
}

void serialize_null_value(JsonSerializer *serializer) {
  write_string(serializer, "null");
}

// Structural serialization helpers
void write_object_start(JsonSerializer *serializer) {
  write_char(serializer, '{');
}

void write_object_end(JsonSerializer *serializer) {
  write_char(serializer, '}');
}

void write_array_start(JsonSerializer *serializer) {
  write_char(serializer, '[');
}

void write_array_end(JsonSerializer *serializer) {
  write_char(serializer, ']');
}

void write_key(JsonSerializer *serializer, const char *key) {
  serialize_string_value(serializer, key);
  write_colon(serializer);
}

void write_comma(JsonSerializer *serializer) { write_char(serializer, ','); }

void write_colon(JsonSerializer *serializer) { write_char(serializer, ':'); }