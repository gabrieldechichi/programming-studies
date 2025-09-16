#ifndef H_STRBUILDER
#define H_STRBUILDER

#include "typedefs.h"

typedef struct {
  char *buffer;
  size_t size;
  size_t len;
} StringBuilder;

void sb_init(StringBuilder *sb, char *buffer, size_t size);

void sb_clear(StringBuilder *sb);

i32 sb_append(StringBuilder *sb, const char *str);

i32 sb_append_space(StringBuilder *sb);
void sb_append_f32(StringBuilder *sb, f64 value, u32 decimal_places);
void sb_append_u32(StringBuilder *sb, u32 value);

char *sb_get(StringBuilder *sb);

size_t sb_length(StringBuilder *sb);

size_t sb_remaining(StringBuilder *sb);


#endif // !H_STRBUILDER
