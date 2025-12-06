#include "lib/typedefs.h"
#include "lib/string.h"
#include "os/os.h"

WASM_IMPORT(_os_is_mobile)
b32 _os_is_mobile();

b32 os_is_mobile() { return _os_is_mobile(); }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

WASM_IMPORT(_os_log_info)
void _os_log_info(const char *message, int length, const char *file_name,
                  int file_name_len, int line_num);
WASM_IMPORT(_os_log_warn)
void _os_log_warn(const char *message, int length, const char *file_name,
                  int file_name_len, int line_num);
WASM_IMPORT(_os_log_error)
void _os_log_error(const char *message, int length, const char *file_name,
                   int file_name_len, int line_num);

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
            const char *file_name, uint32 line_number)
{
  char buffer[1024 * 8];
  size_t msg_len = fmt_string(buffer, sizeof(buffer), fmt, args);
  const size_t file_name_len = str_len(file_name);
  switch (log_level)
  {
  case LOGLEVEL_INFO:
    _os_log_info(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_WARN:
    _os_log_warn(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_ERROR:
    _os_log_error(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  default:
    break;
  }
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number)
{
  os_log(log_level, fmt, args, file_name, line_number);
}

#define OS_WASM_MAX_FILE_OPS 64

struct OsFileOp
{
  i32 js_id;
  b32 in_use;
};

static struct OsFileOp os_wasm_file_ops[OS_WASM_MAX_FILE_OPS];

static OsFileOp *os_wasm_file_op_alloc(void)
{
  for (i32 i = 0; i < OS_WASM_MAX_FILE_OPS; i++)
  {
    if (!os_wasm_file_ops[i].in_use)
    {
      os_wasm_file_ops[i].in_use = true;
      return &os_wasm_file_ops[i];
    }
  }
  return NULL;
}

static void os_wasm_file_op_free(OsFileOp *op)
{
  if (op)
  {
    op->in_use = false;
    op->js_id = 0;
  }
}

WASM_IMPORT(_os_start_read_file)
i32 _os_start_read_file(const char *file_name, int length);
WASM_IMPORT(_os_check_read_file)
i32 _os_check_read_file(i32 op_id);
WASM_IMPORT(_os_get_file_size)
i32 _os_get_file_size(i32 op_id);
WASM_IMPORT(_os_get_file_data)
void _os_get_file_data(i32 op_id, u8 *buffer_ptr, u32 buffer_len);

OsFileOp *os_start_read_file(const char *file_path, TaskSystem *mcr_system)
{
  UNUSED(mcr_system);
  OsFileOp *op = os_wasm_file_op_alloc();
  if (!op)
  {
    return NULL;
  }
  size_t len = str_len(file_path);
  op->js_id = _os_start_read_file(file_path, len);
  return op;
}

OsFileReadState os_check_read_file(OsFileOp *op)
{
  if (!op)
  {
    return OS_FILE_READ_STATE_ERROR;
  }
  return (OsFileReadState)_os_check_read_file(op->js_id);
}

i32 os_get_file_size(OsFileOp *op)
{
  if (!op)
  {
    return -1;
  }
  return _os_get_file_size(op->js_id);
}

b32 os_get_file_data(OsFileOp *op, _out_ PlatformFileData *data,
                     Allocator *allocator)
{
  if (!op)
  {
    return false;
  }
  i32 file_size = _os_get_file_size(op->js_id);
  if (file_size < 0)
  {
    return false;
  }
  data->buffer_len = file_size;
  data->buffer = ALLOC_ARRAY(allocator, u8, file_size);
  if (!data->buffer)
  {
    return false;
  }

  _os_get_file_data(op->js_id, data->buffer, (u32)file_size);
  data->success = true;

  os_wasm_file_op_free(op);
  return true;
}

extern unsigned char __heap_base;

WASM_EXPORT(os_get_heap_base)
void *os_get_heap_base(void)
{
#ifdef DEBUG
  // add 1MB padding on debug builds to support hot reload
  return (&__heap_base + KB(1024));
#else
  return &__heap_base;
#endif
}

void os_quit(void)
{
  LOG_WARN("os_quit: WASM does not support quitting the application");
}

WASM_IMPORT(_platform_audio_write_samples)
void _platform_audio_write_samples(f32 *samples, i32 sample_count);
WASM_IMPORT(_platform_audio_get_sample_rate)
i32 _platform_audio_get_sample_rate();
WASM_IMPORT(_platform_audio_get_samples_needed)
u32 _platform_audio_get_samples_needed();
WASM_IMPORT(_platform_audio_update)
void _platform_audio_update();
WASM_IMPORT(_platform_audio_shutdown)
void _platform_audio_shutdown();

void os_audio_init() {}

void os_audio_shutdown(void) { _platform_audio_shutdown(); }

void os_audio_update(void) { _platform_audio_update(); }

void os_audio_write_samples(f32 *samples, i32 sample_count)
{
  _platform_audio_write_samples(samples, sample_count);
}

i32 os_audio_get_sample_rate() { return _platform_audio_get_sample_rate(); }

u32 os_audio_get_samples_needed()
{
  return _platform_audio_get_samples_needed();
}

WASM_IMPORT(_os_lock_mouse)
void _os_lock_mouse(b32 lock);
WASM_IMPORT(_os_is_mouse_locked)
b32 _os_is_mouse_locked();

void os_lock_mouse(b32 lock) { _os_lock_mouse(lock); }

b32 os_is_mouse_locked(void) { return _os_is_mouse_locked(); }

WASM_IMPORT(_os_get_compressed_texture_format)
i32 _os_get_compressed_texture_format();

const char *os_get_compressed_texture_format_suffix(void)
{
  i32 format = _os_get_compressed_texture_format();
  switch (format)
  {
  case 1:
    return "_dxt5";
  case 2:
    return "_etc2";
  case 3:
    return "_astc";
  case 4:
    return "_etc1";
  default:
    return "";
  }
}
