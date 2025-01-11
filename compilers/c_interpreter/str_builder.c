#ifndef H_STRING_BUILDER
#define H_STRING_BUILDER

#include <stddef.h>
#include <string.h>

typedef void *(*Allocator)(size_t);
typedef void *(*Reallocator)(void *ptr, size_t);

typedef struct {
  char *str;
  size_t length;
  size_t capaciy;
  Reallocator realloc;
} StringBuilder;

int sb_init_cap(StringBuilder *sb, Reallocator realloc, size_t initial_cap) {
  *sb = (StringBuilder){0};
  sb->realloc = realloc;
  sb->capaciy = initial_cap;
  sb->length = 0;
  sb->str = NULL;
  sb->str = sb->realloc(sb->str, sb->capaciy * sizeof(char));
  if (!sb->str) {
    return 1;
  }
  return 0;
}

int sb_init(StringBuilder *sb, Reallocator realloc) {
  return sb_init_cap(sb, realloc, 16);
}

void sb_append_len(StringBuilder *sb, const char *text, size_t text_length) {
  if (sb->length + text_length >= sb->capaciy) {
    sb->capaciy = (sb->capaciy + text_length) * 2;
    sb->str = sb->realloc(sb->str, sb->capaciy);
  }
  strcpy(sb->str + sb->length, text);
  sb->length += text_length;
}

void sb_append(StringBuilder *sb, const char *text) {
  return sb_append_len(sb, text, strlen(text));
}

#endif
