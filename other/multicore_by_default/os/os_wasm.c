#include "lib/typedefs.h"
#include "os/os.h"

extern b32 _os_is_mobile();

b32 os_is_mobile() { return _os_is_mobile(); }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

extern void _os_log_info(const char *message, int length, const char *file_name,
                         int file_name_len, int line_num);
extern void _os_log_warn(const char *message, int length, const char *file_name,
                         int file_name_len, int line_num);
extern void _os_log_error(const char *message, int length,
                          const char *file_name, int file_name_len,
                          int line_num);

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
            const char *file_name, uint32 line_number) {
  char buffer[1024 * 8];
  size_t str_len = fmt_string(buffer, sizeof(buffer), fmt, args);
  const size_t file_name_len = strlen(file_name);
  switch (log_level) {
  case LOGLEVEL_INFO:
    _os_log_info(buffer, str_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_WARN:
    _os_log_warn(buffer, str_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_ERROR:
    _os_log_error(buffer, str_len, file_name, file_name_len, line_number);
    break;
  default:
    break;
  }
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  os_log(log_level, fmt, args, file_name, line_number);
}

extern OsFileReadOp _os_start_read_file(const char *file_name, int length);
extern OsFileReadState _os_check_read_file(OsFileReadOp op_id);
extern int32 _os_get_file_size(OsFileReadOp op_id);
extern void _os_get_file_data(OsFileReadOp op_id, uint8 *buffer_ptr,
                              uint32 buffer_len);

OsFileReadOp os_start_read_file(const char *file_path) {
  size_t len = strlen(file_path);
  return _os_start_read_file(file_path, len);
}

OsFileReadState os_check_read_file(OsFileReadOp op_id) {
  return _os_check_read_file(op_id);
}

int32 os_get_file_size(OsFileReadOp op_id) { return _os_get_file_size(op_id); }

bool32 os_get_file_data(OsFileReadOp op_id, _out_ PlatformFileData *data,
                        Allocator *allocator) {
  int64 file_size = _os_get_file_size(op_id);
  if (file_size < 0) {
    return false;
  }
  data->buffer_len = file_size;
  data->buffer = ALLOC_ARRAY(allocator, uint8, file_size);
  assert(data->buffer);

  _os_get_file_data(op_id, data->buffer, (uintptr_t)file_size);
  data->success = true;
  return true;
}

extern OsWebPLoadOp _os_start_webp_texture_load(const char *file_path,
                                                int length,
                                                u32 texture_handle_idx,
                                                u32 texture_handle_gen);
extern OsFileReadState _os_check_webp_texture_load(OsWebPLoadOp op_id);

OsWebPLoadOp os_start_webp_texture_load(const char *file_path,
                                        u32 file_path_len,
                                        u32 texture_handle_idx,
                                        u32 texture_handle_gen) {
  return _os_start_webp_texture_load(file_path, file_path_len,
                                     texture_handle_idx, texture_handle_gen);
}

OsFileReadState os_check_webp_texture_load(OsWebPLoadOp op_id) {
  return _os_check_webp_texture_load(op_id);
}

extern unsigned char __heap_base;

WASM_EXPORT(os_get_heap_base) void *os_get_heap_base(void) {
  return &__heap_base;
}

void os_quit(void) {
  LOG_WARN("os_quit: WASM does not support quitting the application");
}
