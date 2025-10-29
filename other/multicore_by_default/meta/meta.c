#include "lib/memory.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "parser.h"
#include "tokenizer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib/typedefs.h"

#include "lib/string.c"
#include "lib/memory.c"
#include "lib/common.c"
#include "lib/string_builder.c"
#include "os/os.c"
#include "meta/tokenizer.c"
#include "meta/parser.c"

void print_reflected_struct(ReflectedStruct *s) {
  printf("SUCCESS\n");

  if (s->struct_name.len > 0) {
    printf("Struct name: %.*s\n", s->struct_name.len, s->struct_name.value);
  } else {
    printf("Struct name: <anonymous>\n");
  }

  if (s->typedef_name.len > 0) {
    printf("Typedef name: %.*s\n", s->typedef_name.len, s->typedef_name.value);
  }

  printf("Type ID: %u\n", s->type_id);

  if (s->attributes.len > 0) {
    printf("Struct attributes:\n");
    for (u32 i = 0; i < s->attributes.len; i++) {
      MetaAttribute *attr = &s->attributes.items[i];
      printf("  %.*s()\n", attr->name.len, attr->name.value);
    }
  }

  printf("Fields:\n");
  for (u32 i = 0; i < s->fields.len; i++) {
    StructField *field = &s->fields.items[i];

    if (field->attributes.len > 0) {
      printf("  Field attributes:\n");
      for (u32 j = 0; j < field->attributes.len; j++) {
        MetaAttribute *attr = &field->attributes.items[j];
        printf("    %.*s()\n", attr->name.len, attr->name.value);
      }
    }

    printf("  %.*s", field->type_name.len, field->type_name.value);
    for (u32 j = 0; j < field->pointer_depth; j++) {
      printf("*");
    }
    printf(" %.*s", field->field_name.len, field->field_name.value);
    if (field->is_array) {
      printf("[%u]", field->array_size);
    }
    printf(";\n");
  }
  printf("\n");
}

typedef struct {
  StringBuilder sb;
  u8 indent_level;
} CodeStringBuilder;

CodeStringBuilder csb_create(Allocator *allocator, u32 cap) {
  CodeStringBuilder csb = {0};
  sb_init(&csb.sb, ALLOC_ARRAY(allocator, char, cap), cap);
  return csb;
}

void csb_add_indent(CodeStringBuilder *csb) {
  for (u8 i = 0; i < csb->indent_level; i++) {
    sb_append(&csb->sb, "    ");
  }
}

void csb_append_line(CodeStringBuilder *csb, const char *str) {
  csb_add_indent(csb);
  sb_append_line(&csb->sb, str);
}

#define csb_append_line_format(csb, fmt, ...)                                  \
  do {                                                                         \
    csb_add_indent((csb));                                                     \
    sb_append_line_format(&(csb)->sb, fmt, __VA_ARGS__);                            \
  } while (0);

int main() {
  ArenaAllocator temp_arena = arena_from_buffer(malloc(MB(64)), MB(64));
  Allocator temp_allocator = make_arena_allocator(&temp_arena);

  ArenaAllocator arena = arena_from_buffer(malloc(MB(8)), MB(8));
  Allocator allocator = make_arena_allocator(&arena);

  const char *file_name = "./src/multicore_tasks.c";
  PlatformFileData file = os_read_file(file_name, &allocator);
  if (!file.success) {
    return 1;
  }

  Parser parser = parser_create(file_name, (char *)file.buffer, file.buffer_len,
                                &allocator);

  while (!parser_current_token_is(&parser, TOKEN_EOF) && !parser.has_error) {
    parser_skip_to_next_attribute(&parser);
    if (parser_current_token_is(&parser, TOKEN_IDENTIFIER)) {
      ReflectedStruct s = {0};
      if (parse_struct(&parser, &s)) {
        // code gen
        String struct_name =
            s.typedef_name.len ? s.typedef_name : s.struct_name;

        CodeStringBuilder csb = csb_create(&temp_allocator, MB(1));
        csb_append_line_format(&csb, "void _%_Exec(void* _data) {",
                               FMT_STR(struct_name.value));
        csb.indent_level++;
        csb_append_line_format(&csb, "%* data = (%*)_data;",
                               FMT_STR(struct_name.value),
                               FMT_STR(struct_name.value));
        csb_append_line_format(&csb, "%_Exec(data);",
                               FMT_STR(struct_name.value));
        csb.indent_level--;
        csb_append_line_format(&csb, "}\n");

        char *generated = sb_get(&csb.sb);
        printf("%s\n", generated);

      } else {
        printf("ERROR: %s\n", parser.error_message.value);
      }
    }
    parser_advance_token(&parser);
  }
  parser_destroy(&parser);

  return 0;
}
