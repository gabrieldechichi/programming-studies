#ifndef H_STRBUILDER
#define H_STRBUILDER

#include "lib/typedefs.h"
#include "lib/fmt.h"

typedef struct {
  char *buffer;
  size_t size;
  size_t len;
} StringBuilder;

void sb_init(StringBuilder *sb, char *buffer, size_t size);

void sb_clear(StringBuilder *sb);

i32 sb_append(StringBuilder *sb, const char *str);

i32 sb_append_space(StringBuilder *sb);
i32 sb_append_char(StringBuilder *sb, char c);
i32 sb_append_line(StringBuilder *sb, const char *str);
void sb_append_f32(StringBuilder *sb, f64 value, u32 decimal_places);
void sb_append_u32(StringBuilder *sb, u32 value);

i32 _sb_append_format(StringBuilder *sb, const char *fmt, const FmtArgs *args);

#define sb_append_format(sb, fmt, ...)                                         \
  do {                                                                         \
    FmtArg args[] = {(FmtArg){.type = 0}, ##__VA_ARGS__};                      \
    size_t _count = (sizeof(args) / sizeof(FmtArg)) - 1;                       \
    FmtArgs fmtArgs = {args + 1, (u8)_count};                                  \
    _sb_append_format(sb, fmt, &fmtArgs);                                      \
  } while (0)

#define sb_append_line_format(sb, fmt, ...)                                    \
  do {                                                                         \
    sb_append_format(sb, fmt, ##__VA_ARGS__);                                    \
    sb_append_char(sb, '\n');                                                  \
  } while (0)

char *sb_get(StringBuilder *sb);

size_t sb_length(StringBuilder *sb);

size_t sb_remaining(StringBuilder *sb);

#endif // !H_STRBUILDER
