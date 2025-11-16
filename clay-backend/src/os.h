#ifndef H_OS
#define H_OS

#include "memory.h"
#include "typedefs.h"

typedef struct {
  u32 buffer_len;
  u8 *buffer;
  b32 success;
} PlatformFileData;

typedef enum {
  OS_FILE_READ_STATE_NONE = 0,
  OS_FILE_READ_STATE_IN_PROGRESS = 1,
  OS_FILE_READ_STATE_COMPLETED = 2,
  OS_FILE_READ_STATE_ERROR = 3
} OsFileReadState;

typedef i32 OsFileReadOp;

OsFileReadOp os_start_read_file(const char *file_path);
OsFileReadState os_check_read_file(OsFileReadOp op_id);
i32 os_get_file_size(OsFileReadOp op_id);
b32 os_get_file_data(OsFileReadOp op_id, PlatformFileData *data,
                     ArenaAllocator *allocator);
#endif
