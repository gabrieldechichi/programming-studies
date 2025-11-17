#include "code_builder.h"

CodeStringBuilder csb_create(char *file_name, Allocator *allocator, u32 cap) {
  CodeStringBuilder csb = {0};
  csb.file_name = file_name;
  sb_init(&csb.sb, ALLOC_ARRAY(allocator, char, cap), cap);

  csb_append_line(&csb, "// ==== GENERATED FILE DO NOT EDIT ====\n");
  csb_append_line_format(&csb, "#ifndef H_%_GEN", FMT_STR(csb.file_name));
  csb_append_line_format(&csb, "#define H_%_GEN", FMT_STR(csb.file_name));
  csb_append_line(&csb, "#include <stdarg.h>");
  csb_append_line(&csb, "#include \"lib/task.h\"");
  csb_append_line_format(&csb, "#include \"%.h\"", FMT_STR(csb.file_name));
  csb_append_line(&csb, "");

  return csb;
}

