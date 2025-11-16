#include "../lib/common.h"
#include "os.h"
#include "string.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "os_darwin_common.c"

// Forward declare the iOS helper functions we'll implement at the bottom
const char* ios_get_bundle_resource_path(const char *relative_path);
const char* ios_get_documents_path(const char *relative_path);
void ios_log_message(const char *message);
int ios_get_thermal_state(void);
void ios_install_crash_handlers(void);
void ios_check_pending_crash_report(void);

b32 os_is_mobile() { return true; }
void os_lock_mouse(b32 lock) { UNUSED(lock); }

OSThermalState os_get_thermal_state(void) {
  return (OSThermalState)ios_get_thermal_state();
}

/*!
 * Platform log
 * */
void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
                  const char *file_name, uint32 line_number) {
  char buffer[KB(128)];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "INFO";
    break;
  case LOGLEVEL_WARN:
    level_str = "WARN";
    break;
  case LOGLEVEL_ERROR:
    level_str = "ERROR";
    break;
  default:
    level_str = "UNKNOWN";
    break;
  }

  char log_msg[2048];
  snprintf(log_msg, sizeof(log_msg), "[%s] %s:%u: %s", level_str, file_name, line_number, buffer);
  ios_log_message(log_msg);
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  os_log(log_level, fmt, args, file_name, line_number);
}

/*!
 * File IO
 * */
bool32 os_write_file(const char *file_path, u8 *buffer, size_t buffer_len) {
  const char *full_path = ios_get_documents_path(file_path);

  FILE *file = fopen(full_path, "wb");
  if (file == NULL) {
    LOG_ERROR("Error opening file for writing: %", FMT_STR(file_path));
    return false;
  }

  size_t written = fwrite(buffer, 1, buffer_len, file);
  if (written != buffer_len) {
    LOG_ERROR("Error writing to file: %", FMT_STR(file_path));
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

bool32 os_create_dir(const char *dir_path) {
  const char *full_path = ios_get_documents_path(dir_path);
  
  if (mkdir(full_path, 0755) == 0) {
    return true;
  }
  
  // Check if directory already exists
  struct stat st;
  if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }
  
  LOG_ERROR("Failed to create directory: %", FMT_STR(dir_path));
  return false;
}

PlatformFileData os_read_file(const char *file_path, Allocator *allocator) {
  PlatformFileData result = {0};

  const char *full_path = ios_get_bundle_resource_path(file_path);

  FILE *file = fopen(full_path, "rb");
  if (file == NULL) {
    LOG_ERROR("Failed to open file: %", FMT_STR(file_path));
    return result;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size < 0) {
    LOG_ERROR("Failed to get file size: %", FMT_STR(file_path));
    fclose(file);
    return result;
  }

  result.buffer = ALLOC_ARRAY(allocator, uint8, file_size);
  if (!result.buffer) {
    LOG_ERROR("Failed to allocate memory for file: %", FMT_STR(file_path));
    fclose(file);
    return result;
  }

  size_t bytes_read = fread(result.buffer, 1, file_size, file);
  fclose(file);

  if (bytes_read != (size_t)file_size) {
    LOG_ERROR("Failed to read entire file: %", FMT_STR(file_path));
    result.buffer = NULL;
    result.buffer_len = 0;
    return result;
  }

  result.buffer_len = file_size;
  result.success = true;
  return result;
}

OsFileReadOp os_start_read_file(const char *file_path) {
  UNUSED(file_path);
  assert_msg(false, "Async file read not supported on native platforms");
  return -1;
}

OsFileReadState os_check_read_file(OsFileReadOp op_id) {
  UNUSED(op_id);
  assert_msg(false, "Async file read not supported on native platforms");
  return OS_FILE_READ_STATE_ERROR;
}

int32 os_get_file_size(OsFileReadOp op_id) {
  UNUSED(op_id);
  assert_msg(false, "Async file read not supported on native platforms");
  return -1;
}

bool32 os_get_file_data(OsFileReadOp op_id, _out_ PlatformFileData *data,
                        Allocator *allocator) {
  UNUSED(op_id);
  UNUSED(data);
  UNUSED(allocator);
  assert_msg(false, "Async file read not supported on native platforms");
  return false;
}

OsDynLib os_dynlib_load(const char* path) {
  UNUSED(path);
  return NULL;
}

void os_dynlib_unload(OsDynLib lib) {
  UNUSED(lib);
}

OsDynSymbol os_dynlib_get_symbol(OsDynLib lib, const char* symbol_name) {
  UNUSED(lib);
  UNUSED(symbol_name);
  return NULL;
}

OsFileInfo os_file_info(const char* path) {
  OsFileInfo info = {0};
  const char *full_path = ios_get_bundle_resource_path(path);
  struct stat file_stat;
  if (stat(full_path, &file_stat) == 0) {
    info.modification_time = file_stat.st_mtime;
    info.exists = true;
  } else {
    info.exists = false;
  }
  return info;
}

b32 os_file_exists(const char* path) {
  const char *full_path = ios_get_bundle_resource_path(path);
  struct stat file_stat;
  return stat(full_path, &file_stat) == 0;
}

b32 os_directory_copy(const char* src_path, const char* dst_path) {
  UNUSED(src_path);
  UNUSED(dst_path);
  return false;
}

b32 os_directory_remove(const char* path) {
  UNUSED(path);
  return false;
}

OsFileList os_list_files(const char* directory, const char* extension, Allocator* allocator) {
  OsFileList result = {0};
  UNUSED(directory);
  UNUSED(extension);
  UNUSED(allocator);
  return result;
}

void os_install_crash_handler(void) {
  ios_check_pending_crash_report();
  ios_install_crash_handlers();
}

