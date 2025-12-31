#include "lib/typedefs.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "lib/array.h"
#include "lib/hash.h"
#include "lib/assert.h"
#include "lib/thread_context.h"
#include "lib/multicore_runtime.h"
#include "os/os.h"

#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/cmd_line.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/task.c"
#include "lib/multicore_runtime.c"
#include "os/os_win32.c"

typedef struct {
    String_DynArray visited;
    Allocator *alloc;
} IncludeContext;

static b32 is_file_visited(IncludeContext *ctx, String path) {
    for (u32 i = 0; i < ctx->visited.len; i++) {
        if (str_equal_len(ctx->visited.items[i].value, ctx->visited.items[i].len,
                          path.value, path.len)) {
            return true;
        }
    }
    return false;
}

static void get_directory(String path, char *out_dir, u32 *out_len) {
    *out_len = 0;
    for (u32 i = path.len; i > 0; i--) {
        if (path.value[i - 1] == '/' || path.value[i - 1] == '\\') {
            *out_len = i;
            break;
        }
    }
    if (*out_len > 0) {
        memcpy(out_dir, path.value, *out_len);
    }
    out_dir[*out_len] = '\0';
}

static b32 parse_include_directive(const char *line, u32 line_len, char *out_path, u32 *out_path_len) {
    const char *include_prefix = "#include";
    u32 prefix_len = 8;

    u32 i = 0;
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    if (line_len - i < prefix_len) {
        return false;
    }

    for (u32 j = 0; j < prefix_len; j++) {
        if (line[i + j] != include_prefix[j]) {
            return false;
        }
    }
    i += prefix_len;

    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    if (i >= line_len || line[i] != '"') {
        return false;
    }
    i++;

    u32 path_start = i;
    while (i < line_len && line[i] != '"') {
        i++;
    }

    if (i >= line_len) {
        return false;
    }

    *out_path_len = i - path_start;
    memcpy(out_path, line + path_start, *out_path_len);
    out_path[*out_path_len] = '\0';

    return true;
}

static b32 process_shader_file(String file_path, StringBuilder *output, IncludeContext *ctx);

static b32 process_line(const char *line, u32 line_len, String current_file_path,
                        StringBuilder *output, IncludeContext *ctx) {
    char include_path[256];
    u32 include_path_len;

    if (parse_include_directive(line, line_len, include_path, &include_path_len)) {
        char current_dir[512];
        u32 current_dir_len;
        get_directory(current_file_path, current_dir, &current_dir_len);

        char full_include_path[512];
        StringBuilder path_sb;
        sb_init(&path_sb, full_include_path, sizeof(full_include_path));
        sb_append_len(&path_sb, current_dir, current_dir_len);
        sb_append_len(&path_sb, include_path, include_path_len);

        String include_str = {
            .value = ALLOC_ARRAY(ctx->alloc, char, sb_length(&path_sb) + 1),
            .len = (u32)sb_length(&path_sb)
        };
        memcpy(include_str.value, full_include_path, include_str.len);
        include_str.value[include_str.len] = '\0';

        if (is_file_visited(ctx, include_str)) {
            return true;
        }

        arr_append(ctx->visited, include_str);

        if (!process_shader_file(include_str, output, ctx)) {
            LOG_ERROR("Failed to process include: %", FMT_STR(full_include_path));
            return false;
        }

        return true;
    }

    sb_append_len(output, line, line_len);
    sb_append(output, "\n");
    return true;
}

static b32 process_shader_file(String file_path, StringBuilder *output, IncludeContext *ctx) {
    char path_cstr[512];
    memcpy(path_cstr, file_path.value, file_path.len);
    path_cstr[file_path.len] = '\0';

    PlatformFileData file_data = os_read_file(path_cstr, ctx->alloc);
    if (!file_data.success) {
        LOG_ERROR("Failed to read shader file: %", FMT_STR(path_cstr));
        return false;
    }

    const char *content = (const char *)file_data.buffer;
    u64 content_len = file_data.buffer_len;

    u64 line_start = 0;
    for (u64 i = 0; i <= content_len; i++) {
        b32 is_end = (i == content_len);
        b32 is_newline = !is_end && (content[i] == '\n' || content[i] == '\r');

        if (is_end || is_newline) {
            u32 line_len = (u32)(i - line_start);

            if (line_len > 0) {
                if (!process_line(content + line_start, line_len, file_path, output, ctx)) {
                    return false;
                }
            } else {
                sb_append(output, "\n");
            }

            if (is_newline && i + 1 < content_len && content[i] == '\r' && content[i + 1] == '\n') {
                i++;
            }
            line_start = i + 1;
        }
    }

    return true;
}

static void write_header_file(const char *output_path, const char *var_name,
                              const u8 *data, u64 data_len, Allocator *alloc) {
    u64 estimated_size = KB(1) + data_len * 10;
    char *buffer = ALLOC_ARRAY(alloc, char, estimated_size);
    StringBuilder sb;
    sb_init(&sb, buffer, estimated_size);

    sb_append(&sb, "#pragma once\n\n");
    sb_append(&sb, "static const unsigned char ");
    sb_append(&sb, var_name);
    sb_append(&sb, "[] = {\n    ");

    for (u64 i = 0; i < data_len; i++) {
        char hex[8];
        StringBuilder hex_sb;
        sb_init(&hex_sb, hex, sizeof(hex));
        sb_append(&hex_sb, "0x");

        u8 byte = data[i];
        char hex_chars[] = "0123456789abcdef";
        char byte_hex[3];
        byte_hex[0] = hex_chars[(byte >> 4) & 0xF];
        byte_hex[1] = hex_chars[byte & 0xF];
        byte_hex[2] = '\0';
        sb_append(&hex_sb, byte_hex);

        sb_append(&sb, hex);
        if (i < data_len - 1) {
            sb_append(&sb, ", ");
            if ((i + 1) % 12 == 0) {
                sb_append(&sb, "\n    ");
            }
        }
    }

    sb_append(&sb, "\n};\n\n");
    sb_append(&sb, "static const unsigned int ");
    sb_append(&sb, var_name);
    sb_append(&sb, "_len = sizeof(");
    sb_append(&sb, var_name);
    sb_append(&sb, ") - 1;\n");

    b32 success = os_write_file(output_path, (u8 *)buffer, sb_length(&sb));
    if (success) {
        LOG_INFO("Wrote shader header: %", FMT_STR(output_path));
    } else {
        LOG_ERROR("Failed to write shader header: %", FMT_STR(output_path));
    }
}

local_shared i32 g_argc;
local_shared char **g_argv;

void print_usage(void) {
    LOG_INFO("Usage: shader_compiler --input <shader.wgsl> --output <shader.h> --name <var_name>");
    LOG_INFO("Options:");
    LOG_INFO("  --input   Path to input .wgsl file");
    LOG_INFO("  --output  Path to output .h file");
    LOG_INFO("  --name    Variable name for the shader data");
}

void entrypoint(void) {
    local_shared Allocator allocator;
    local_shared ArenaAllocator arena;
    local_shared b32 parse_success;
    local_shared String input_path;
    local_shared String output_path;
    local_shared String var_name;

    if (is_main_thread()) {
        os_time_init();

        const u64 arena_size = MB(64);
        void *memory = os_allocate_memory(arena_size);
        arena = arena_from_buffer(memory, arena_size);
        allocator = make_arena_allocator(&arena);

        CmdLineParser parser = cmdline_create(&allocator);

        cmdline_add_option(&parser, "input");
        cmdline_add_option(&parser, "output");
        cmdline_add_option(&parser, "name");

        parse_success = cmdline_parse(&parser, g_argc, g_argv);

        if (parse_success) {
            input_path = cmdline_get_option(&parser, "input");
            output_path = cmdline_get_option(&parser, "output");
            var_name = cmdline_get_option(&parser, "name");

            if (input_path.len == 0 || output_path.len == 0 || var_name.len == 0) {
                LOG_ERROR("Missing required options");
                print_usage();
                parse_success = false;
            }
        }

        if (!parse_success) {
            print_usage();
        }
    }
    lane_sync();

    if (!parse_success) {
        return;
    }

    if (is_main_thread()) {
        LOG_INFO("Shader compiler started");
        LOG_INFO("  Input:  %", FMT_STR_VIEW(input_path));
        LOG_INFO("  Output: %", FMT_STR_VIEW(output_path));
        LOG_INFO("  Name:   %", FMT_STR_VIEW(var_name));
    }
    lane_sync();

    if (!is_main_thread()) {
        return;
    }

    ThreadContext *tctx = tctx_current();
    Allocator temp_alloc = make_arena_allocator(&tctx->temp_arena);

    u64 output_buffer_size = MB(512);
    char *output_buffer = ALLOC_ARRAY(&temp_alloc, char, output_buffer_size);
    StringBuilder output_sb;
    sb_init(&output_sb, output_buffer, output_buffer_size);

    IncludeContext ctx = {
        .visited = dyn_arr_new_alloc(&temp_alloc, String, 32),
        .alloc = &temp_alloc
    };

    char *input_copy = ALLOC_ARRAY(&temp_alloc, char, input_path.len + 1);
    for (u32 i = 0; i < input_path.len; i++) {
        input_copy[i] = input_path.value[i];
    }
    input_copy[input_path.len] = '\0';
    String input_str = { .value = input_copy, .len = input_path.len };
    arr_append(ctx.visited, input_str);
    if (!process_shader_file(input_path, &output_sb, &ctx)) {
        LOG_ERROR("Failed to process shader");
        return;
    }

    char output_cstr[512];
    memcpy(output_cstr, output_path.value, output_path.len);
    output_cstr[output_path.len] = '\0';

    char name_cstr[256];
    memcpy(name_cstr, var_name.value, var_name.len);
    name_cstr[var_name.len] = '\0';

    write_header_file(output_cstr, name_cstr,
                      (const u8 *)output_buffer, sb_length(&output_sb) + 1,
                      &temp_alloc);

    LOG_INFO("Processed % include files", FMT_UINT(ctx.visited.len));
}

int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;

    os_init();
    i32 num_cores = 1;
    i32 thread_count = MAX(1, num_cores);

    const u64 runtime_arena_size = GB(4);
    void *runtime_memory = os_allocate_memory(runtime_arena_size);
    ArenaAllocator runtime_arena = arena_from_buffer(runtime_memory, runtime_arena_size);

    mcr_run((u8)thread_count, GB(1), entrypoint, &runtime_arena);

    return 0;
}
