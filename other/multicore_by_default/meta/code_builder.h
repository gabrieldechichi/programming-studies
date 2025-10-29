#ifndef H_CODE_BUILDER
#define H_CODE_BUILDER
#include "lib/memory.h"
#include "lib/string_builder.h"

typedef struct {
  char *file_name;
  StringBuilder sb;
  u8 indent_level;
} CodeStringBuilder;

CodeStringBuilder csb_create(char *file_name, Allocator *allocator, u32 cap);

force_inline void csb_add_indent(CodeStringBuilder *csb) {
  csb->indent_level++;
}
force_inline void csb_remove_indent(CodeStringBuilder *csb) {
  if (csb->indent_level > 0) {
    csb->indent_level--;
  }
}

force_inline void csb_append_indentation(CodeStringBuilder *csb) {
  for (u8 i = 0; i < csb->indent_level; i++) {
    sb_append(&csb->sb, "    ");
  }
}

force_inline void csb_append_line(CodeStringBuilder *csb, const char *str) {
  csb_append_indentation(csb);
  sb_append_line(&csb->sb, str);
}

force_inline char *csb_finish(CodeStringBuilder *csb) {
  csb_append_line(csb, "#endif");
  csb_append_line(csb, "// ==== GENERATED FILE DO NOT EDIT ====\n");

  return sb_get(&csb->sb);
}

#define csb_append_line_format(csb, fmt, ...)                                  \
  do {                                                                         \
    csb_append_indentation((csb));                                             \
    sb_append_line_format(&(csb)->sb, fmt, __VA_ARGS__);                       \
  } while (0);

#endif
