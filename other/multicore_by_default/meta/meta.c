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
#include "meta/code_builder.h"

#include "lib/string.c"
#include "lib/memory.c"
#include "lib/common.c"
#include "lib/string_builder.c"
#include "os/os.c"
#include "meta/tokenizer.c"
#include "meta/parser.c"
#include "meta/code_builder.c"

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

  CodeStringBuilder csb = csb_create("multicore_tasks", &temp_allocator, MB(1));

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

  char *generated = csb_finish(&csb);

  os_create_dir("./generated");
  char temp_buffer[512];
  FMT_TO_STR(temp_buffer, sizeof(temp_buffer), "./generated/%_generated.h",
             FMT_STR(csb.file_name));
  os_write_file(temp_buffer, (u8 *)generated, csb.sb.len);
  parser_destroy(&parser);

  return 0;
}
