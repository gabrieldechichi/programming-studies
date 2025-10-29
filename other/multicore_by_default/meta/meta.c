#include "lib/memory.h"
#include "lib/string.h"
#include "parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include "lib/typedefs.h"

#include "lib/string.c"
#include "lib/memory.c"
#include "lib/common.c"
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
    printf(" %.*s;\n", field->field_name.len, field->field_name.value);
  }
  printf("\n");
}

int main() {
  ArenaAllocator arena = arena_from_buffer(malloc(MB(8)), MB(8));
  Allocator allocator = make_arena_allocator(&arena);

  const char *file_name = "./src/multicore_tasks.c";
  PlatformFileData file = os_read_file(file_name, &allocator);
  if (!file.success) {
    return 1;
  }

  const char *source = "\
HZ_TASK()\n\
typedef struct {\n\
  HZ_READ()\n\
  u64 values_start;\n\
\n\
  HZ_WRITE()\n\
  i64 numbers;\n\
} TaskWideSumInit;\n";

  Parser parser = parser_create(file_name, (char *)file.buffer, file.buffer_len,
                                &allocator);

  while (!parser_current_token_is(&parser, TOKEN_EOF) && !parser.has_error) {
    if (parser_current_token_is(&parser, TOKEN_STRUCT)) {
      ReflectedStruct s = {0};
      if (parse_struct(&parser, &s)) {
        print_reflected_struct(&s);
      } else {
        printf("ERROR: %s\n", parser.error_message.value);
      }
    }
    parser_advance_token(&parser);
  }
  parser_destroy(&parser);

  return 0;
}
