#include "os.h"
#include "str.h"
#include "assert.h"

extern OsFileReadOp _os_start_read_file(const char *file_name, int length);
extern OsFileReadState _os_check_read_file(OsFileReadOp op_id);
extern i32 _os_get_file_size(OsFileReadOp op_id);
extern void _os_get_file_data(OsFileReadOp op_id, uint8 *buffer_ptr,
                              uint32 buffer_len);

OsFileReadOp os_start_read_file(const char *file_path) {
  size_t len = strlen(file_path);
  return _os_start_read_file(file_path, len);
}

OsFileReadState os_check_read_file(OsFileReadOp op_id) {
  return _os_check_read_file(op_id);
}

i32 os_get_file_size(OsFileReadOp op_id) { return _os_get_file_size(op_id); }

b32 os_get_file_data(OsFileReadOp op_id, PlatformFileData *data,
                     ArenaAllocator *allocator) {
  int64 file_size = _os_get_file_size(op_id);
  if (file_size < 0) {
    return false;
  }
  data->buffer_len = file_size;
  data->buffer = arena_alloc(allocator, file_size);
  assert(data->buffer);

  _os_get_file_data(op_id, data->buffer, (uintptr_t)file_size);
  data->success = true;
  return true;
}
