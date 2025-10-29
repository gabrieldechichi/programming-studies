#include "lib/memory.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "os.h"
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
      StructAttribute *attr = &s->attributes.items[i];
      printf("  %.*s()\n", attr->name.len, attr->name.value);
    }
  }

  printf("Fields:\n");
  for (u32 i = 0; i < s->fields.len; i++) {
    StructField *field = &s->fields.items[i];

    if (field->attributes.len > 0) {
      printf("  Field attributes:\n");
      for (u32 j = 0; j < field->attributes.len; j++) {
        FieldAttribute *attr = &field->attributes.items[j];
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

void csb_add_indent(CodeStringBuilder *csb) { csb->indent_level++; }
void csb_remove_indent(CodeStringBuilder *csb) {
  if (csb->indent_level > 0) {
    csb->indent_level--;
  }
}

void csb_append_indentation(CodeStringBuilder *csb) {
  for (u8 i = 0; i < csb->indent_level; i++) {
    sb_append(&csb->sb, "    ");
  }
}

void csb_append_line(CodeStringBuilder *csb, const char *str) {
  csb_append_indentation(csb);
  sb_append_line(&csb->sb, str);
}

char *csb_get(CodeStringBuilder *csb) { return sb_get(&csb->sb); }

#define csb_append_line_format(csb, fmt, ...)                                  \
  do {                                                                         \
    csb_append_indentation((csb));                                             \
    sb_append_line_format(&(csb)->sb, fmt, __VA_ARGS__);                       \
  } while (0);

int main() {
  ArenaAllocator temp_arena = arena_from_buffer(malloc(MB(64)), MB(64));
  Allocator temp_allocator = make_arena_allocator(&temp_arena);

  ArenaAllocator arena = arena_from_buffer(malloc(MB(8)), MB(8));
  Allocator allocator = make_arena_allocator(&arena);

  const char *file_name = "./src/multicore_tasks.h";
  PlatformFileData file = os_read_file(file_name, &allocator);
  if (!file.success) {
    return 1;
  }

  Parser parser = parser_create(file_name, (char *)file.buffer, file.buffer_len,
                                &allocator);

  CodeStringBuilder csb = csb_create(&temp_allocator, MB(1));

  csb_append_line_format(&csb, "#ifndef H_%_GEN", FMT_STR("multicore_tasks"));
  csb_append_line_format(&csb, "#define H_%_GEN", FMT_STR("multicore_tasks"));
  csb_append_line(&csb, "#include \"lib/task.h\"");
  csb_append_line(&csb, "#include \"multicore_tasks.h\"");
  csb_append_line(&csb, "");

  while (!parser_current_token_is(&parser, TOKEN_EOF) && !parser.has_error) {
    parser_skip_to_next_attribute(&parser);
    if (parser_current_token_is(&parser, TOKEN_IDENTIFIER)) {
      ReflectedStruct s = {0};
      if (parse_struct(&parser, &s)) {
        // code gen
        String struct_name =
            s.typedef_name.len ? s.typedef_name : s.struct_name;

        // collect attributes
        FieldAttribute_DynArray field_attributes =
            dyn_arr_new_alloc(&temp_allocator, FieldAttribute, s.fields.len);

        arr_foreach_ptr(s.fields, StructField, field) {
          if (field->attributes.len > 0) {
            arr_foreach_ptr(field->attributes, FieldAttribute, attr) {
              if (str_equal(attr->name.value, "HZ_WRITE") ||
                  str_equal(attr->name.value, "HZ_READ")) {
                arr_append(field_attributes, *attr);
              }
            }
          }
        }
        // todo: validate all attributes are provided

        // start code gen
        // Exec wrapper (for safe casting)
        csb_append_line_format(&csb, "void _%_Exec(void* _data) {",
                               FMT_STR(struct_name.value));
        csb_add_indent(&csb);
        csb_append_line_format(&csb, "%* data = (%*)_data;",
                               FMT_STR(struct_name.value),
                               FMT_STR(struct_name.value));
        csb_append_line_format(&csb, "%_Exec(data);",
                               FMT_STR(struct_name.value));
        csb_remove_indent(&csb);
        csb_append_line_format(&csb, "}\n");

        // Schedule function
        csb_append_line_format(&csb,
                               "TaskHandle _%_Schedule(TaskQueue* queue, %* "
                               "data, TaskHandle* deps, u8 deps_count) {",
                               FMT_STR(struct_name.value),
                               FMT_STR(struct_name.value));
        csb_add_indent(&csb);
        csb_append_line_format(&csb, "TaskResourceAccess resource_access[%];",
                               FMT_UINT(field_attributes.len));

        for (u32 i = 0; i < field_attributes.len; i++) {
          FieldAttribute attr = field_attributes.items[i];
          b32 is_write = str_equal(attr.name.value, "HZ_WRITE");
          const char *task_access_macro =
              is_write ? "TASK_ACCESS_WRITE" : "TASK_ACCESS_READ";
          csb_append_line_format(&csb,
                                 "resource_access[%] = %(data->%.items, "
                                 "data->%.len);",
                                 FMT_UINT(i), FMT_STR(task_access_macro),
                                 FMT_STR(attr.parent_field->field_name.value),
                                 FMT_STR(attr.parent_field->field_name.value));
        }
        csb_append_line_format(
            &csb,
            "return _task_queue_append(queue, _%_Exec, data, "
            "resource_access, %, deps, deps_count);",
            FMT_STR(struct_name.value), FMT_UINT(field_attributes.len));
        csb_remove_indent(&csb);
        csb_append_line(&csb, "}\n");

        // helper macro
        csb_append_line_format(
            &csb,
            "#define %_Schedule(queue,data,...) "
            "_%_Schedule(queue,data,ARGS_ARRAY(TaskHandle, __VA_ARGS__), "
            "ARGS_COUNT(TaskHandle, __VA_ARGS__))",
            FMT_STR(struct_name.value), FMT_STR(struct_name.value));
        csb_append_line(&csb, "");
      } else {
        printf("ERROR: %s\n", parser.error_message.value);
      }
    }
  }

  csb_append_line(&csb, "#endif");
  char *generated = csb_get(&csb);

  os_create_dir("./generated");
  char temp_buffer[512];
  FMT_TO_STR(temp_buffer, sizeof(temp_buffer), "./generated/%_generated.h",
             FMT_STR("multicore_tasks"));
  os_write_file(temp_buffer, (u8 *)generated, csb.sb.len);
  parser_destroy(&parser);

  return 0;
}
