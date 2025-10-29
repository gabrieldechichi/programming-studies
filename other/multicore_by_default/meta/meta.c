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

int main() {
  const char *source = "struct Point { \
        int x; \
        int y; \
    }";

  ArenaAllocator arena = arena_from_buffer(malloc(MB(8)), MB(8));
  Allocator allocator = make_arena_allocator(&arena);
  Parser parser = parser_create("test", source, str_len(source), &allocator);

  ReflectedStruct reflected_struct = {0};
  b32 success = parse_struct(&parser, &reflected_struct);

  if (success) {
    printf("SUCCESS\n");
    printf("Struct name: %.*s\n", reflected_struct.struct_name.len,
           reflected_struct.struct_name.value);
    printf("Type ID: %u\n", reflected_struct.type_id);
    printf("Fields:\n");
    for (u32 i = 0; i < reflected_struct.fields.len; i++) {
      StructField *field = &reflected_struct.fields.items[i];
      printf("  %.*s %.*s;\n", field->type_name.len, field->type_name.value,
             field->field_name.len, field->field_name.value);
    }
  } else {
    printf("ERROR: %s\n", parser.error_message.value);
  }

  parser_destroy(&parser);

  return 0;
}
