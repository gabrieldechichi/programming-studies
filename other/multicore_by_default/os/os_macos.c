#undef internal

#include "os.h"
#include "string.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <sys/utsname.h>
#include <pwd.h>

#include "os_darwin_common.c"

b32 os_is_mobile() { return false; }
void os_lock_mouse(b32 lock) { UNUSED(lock); }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

#define MAX_STACK_FRAMES 50
#define MAX_SYMBOL_LEN 512

typedef struct {
  const char *path;
  void *base;
  void *addrs[MAX_STACK_FRAMES];
  int addr_count;
  char *resolved_lines[MAX_STACK_FRAMES];
} BinaryInfo;

static void capture_and_print_stacktrace(FILE *output, int skip_frames) {
  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count = backtrace(stack_frames, MAX_STACK_FRAMES);

  if (frame_count <= skip_frames) {
    return;
  }

  fprintf(output, "\n===== STACK TRACE =====\n");

  char **symbols = backtrace_symbols(stack_frames, frame_count);
  if (!symbols) {
    backtrace_symbols_fd(stack_frames + skip_frames, frame_count - skip_frames,
                         fileno(output));
    fprintf(output, "=======================\n");
    return;
  }

  BinaryInfo binaries[16];
  int binary_count = 0;
  int frame_to_binary[MAX_STACK_FRAMES];

  for (int i = skip_frames; i < frame_count; i++) {
    frame_to_binary[i] = -1;

    Dl_info info;
    if (dladdr(stack_frames[i], &info) && info.dli_fname) {
      int binary_idx = -1;
      for (int j = 0; j < binary_count; j++) {
        if (strcmp(binaries[j].path, info.dli_fname) == 0) {
          binary_idx = j;
          break;
        }
      }

      if (binary_idx == -1 && binary_count < 16) {
        binary_idx = binary_count++;
        binaries[binary_idx].path = info.dli_fname;
        binaries[binary_idx].base = info.dli_fbase;
        binaries[binary_idx].addr_count = 0;
      }

      if (binary_idx >= 0) {
        frame_to_binary[i] = binary_idx;
        binaries[binary_idx].addrs[binaries[binary_idx].addr_count++] =
            stack_frames[i];
      }
    }
  }

  for (int b = 0; b < binary_count; b++) {
    if (binaries[b].addr_count == 0)
      continue;

    char atos_cmd[4096];
    int cmd_len = snprintf(atos_cmd, sizeof(atos_cmd), "atos -o %s -l %p",
                           binaries[b].path, binaries[b].base);

    for (int i = 0; i < binaries[b].addr_count; i++) {
      cmd_len += snprintf(atos_cmd + cmd_len, sizeof(atos_cmd) - cmd_len, " %p",
                          binaries[b].addrs[i]);
    }

    strncat(atos_cmd, " 2>/dev/null", sizeof(atos_cmd) - strlen(atos_cmd) - 1);

    FILE *atos_pipe = popen(atos_cmd, "r");
    if (atos_pipe) {
      char line_info[256];
      int line_idx = 0;

      while (fgets(line_info, sizeof(line_info), atos_pipe) &&
             line_idx < binaries[b].addr_count) {
        char *newline = strchr(line_info, '\n');
        if (newline)
          *newline = '\0';

        size_t len = strlen(line_info) + 1;
        binaries[b].resolved_lines[line_idx] = malloc(len);
        if (binaries[b].resolved_lines[line_idx]) {
          strncpy(binaries[b].resolved_lines[line_idx], line_info, len);
        }
        line_idx++;
      }
      pclose(atos_pipe);
    }
  }

  for (int i = skip_frames; i < frame_count; i++) {
    fprintf(output, "  [%2d] ", i - skip_frames);

    int binary_idx = frame_to_binary[i];
    if (binary_idx >= 0) {
      int addr_idx = -1;
      for (int j = 0; j < binaries[binary_idx].addr_count; j++) {
        if (binaries[binary_idx].addrs[j] == stack_frames[i]) {
          addr_idx = j;
          break;
        }
      }

      if (addr_idx >= 0 && binaries[binary_idx].resolved_lines[addr_idx]) {
        fprintf(output, "%s\n", binaries[binary_idx].resolved_lines[addr_idx]);
      } else {
        Dl_info info;
        if (dladdr(stack_frames[i], &info)) {
          const char *lib_name = info.dli_fname ? info.dli_fname : "???";
          const char *base_name = lib_name;
          for (const char *p = lib_name; *p; p++) {
            if (*p == '/')
              base_name = p + 1;
          }

          if (info.dli_sname) {
            fprintf(output, "%s: %s + %ld\n", base_name, info.dli_sname,
                    (long)((char *)stack_frames[i] - (char *)info.dli_saddr));
          } else {
            fprintf(output, "%s: %p\n", base_name, stack_frames[i]);
          }
        } else {
          fprintf(output, "%s\n", symbols[i]);
        }
      }
    } else {
      fprintf(output, "%s\n", symbols[i]);
    }
  }

  for (int b = 0; b < binary_count; b++) {
    for (int i = 0; i < binaries[b].addr_count; i++) {
      if (binaries[b].resolved_lines[i]) {
        free(binaries[b].resolved_lines[i]);
      }
    }
  }

  free(symbols);
  fprintf(output, "=======================\n");
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  os_log(log_level, fmt, args, file_name, line_number);
}

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
            const char *file_name, uint32 line_number) {
  char buffer[1024];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  const char *color_start = "";
  const char *color_end = "";
  FILE *output;

  b32 use_color = false;

  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "INFO";
    output = stdout;
    use_color = isatty(fileno(stdout));
    break;
  case LOGLEVEL_WARN:
    level_str = "WARN";
    output = stderr;
    use_color = isatty(fileno(stderr));
    if (use_color) {
      color_start = "\033[33m";
      color_end = "\033[0m";
    }
    break;
  case LOGLEVEL_ERROR:
    level_str = "ERROR";
    output = stderr;
    use_color = isatty(fileno(stderr));
    if (use_color) {
      color_start = "\033[31m";
      color_end = "\033[0m";
    }
    break;
  default:
    level_str = "UNKNOWN";
    output = stderr;
    use_color = isatty(fileno(stderr));
    break;
  }

  fprintf(output, "%s[%s] %s:%u: %s%s\n", color_start, level_str, file_name,
          line_number, buffer, color_end);

  // if (log_level == LOGLEVEL_ERROR) {
  //   capture_and_print_stacktrace(output, 2);
  // }

  fflush(output);
}

bool32 os_write_file(const char *file_path, u8 *buffer, size_t buffer_len) {
  FILE *file = fopen(file_path, "wb");
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
  if (mkdir(dir_path, 0755) == 0) {
    return true;
  }

  struct stat st;
  if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  LOG_ERROR("Failed to create directory: %", FMT_STR(dir_path));
  return false;
}

PlatformFileData os_read_file(const char *file_path, Allocator *allocator) {
  PlatformFileData result = {0};

  FILE *file = fopen(file_path, "rb");
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

OsDynLib os_dynlib_load(const char *path) { return dlopen(path, RTLD_NOW); }

void os_dynlib_unload(OsDynLib lib) {
  if (lib) {
    dlclose(lib);
  }
}

OsDynSymbol os_dynlib_get_symbol(OsDynLib lib, const char *symbol_name) {
  if (!lib)
    return NULL;
  return dlsym(lib, symbol_name);
}

OsFileInfo os_file_info(const char *path) {
  OsFileInfo info = {0};
  struct stat file_stat;
  if (stat(path, &file_stat) == 0) {
    info.modification_time = file_stat.st_mtime;
    info.exists = true;
  } else {
    info.exists = false;
  }
  return info;
}

b32 os_file_exists(const char *path) {
  struct stat file_stat;
  return stat(path, &file_stat) == 0;
}

b32 os_directory_copy(const char *src_path, const char *dst_path) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s' 2>/dev/null", src_path, dst_path);
  return os_system(cmd);
}

b32 os_directory_remove(const char *path) {
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", path);
  return os_system(cmd);
}

OsFileList os_list_files(const char *directory, const char *extension,
                         Allocator *allocator) {
  OsFileList result = {0};
  UNUSED(allocator); // We'll use malloc for now to match other implementations

  char **temp_paths = malloc(sizeof(char *) * 256);
  if (!temp_paths)
    return result;

  DIR *dir = opendir(directory);
  if (!dir) {
    free(temp_paths);
    return result;
  }

  int count = 0;
  int capacity = 256;
  size_t ext_len = strlen(extension);

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && count < capacity) {
    if (entry->d_type == DT_REG) {
      size_t name_len = strlen(entry->d_name);
      if (name_len >= ext_len &&
          strcmp(entry->d_name + name_len - ext_len, extension) == 0) {
        size_t path_len = strlen(directory) + name_len + 2;
        char *full_path = malloc(path_len);
        if (full_path) {
          snprintf(full_path, path_len, "%s/%s", directory, entry->d_name);
          temp_paths[count++] = full_path;
        }
      }
    }
  }

  closedir(dir);

  if (count > 0) {
    result.paths = malloc(sizeof(char *) * count);
    if (result.paths) {
      for (int i = 0; i < count; i++) {
        result.paths[i] = temp_paths[i];
      }
      result.count = count;
    } else {
      for (int i = 0; i < count; i++) {
        free(temp_paths[i]);
      }
    }
  }

  free(temp_paths);
  return result;
}

static const char *get_crash_log_dir(void) {
  static char path[1024];
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_dir) {
    snprintf(path, sizeof(path), "%s/Library/Logs/hz-engine", pw->pw_dir);
  } else {
    snprintf(path, sizeof(path), "/tmp/hz-engine-logs");
  }
  return path;
}

static const char *get_crash_log_path(void) {
  static char path[1024];
  snprintf(path, sizeof(path), "%s/crash.log", get_crash_log_dir());
  return path;
}

static void write_crash_info_to_file(int signal_number) {
  const char *log_dir = get_crash_log_dir();
  mkdir(log_dir, 0755);

  const char *crash_log_path = get_crash_log_path();
  FILE *crash_file = fopen(crash_log_path, "w");
  if (!crash_file) {
    crash_file = stderr;
  }

  time_t now;
  time(&now);
  struct tm *tm_info = localtime(&now);
  char time_buffer[64];
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

  struct utsname sys_info;
  uname(&sys_info);

  const char *signal_name = "UNKNOWN";
  switch (signal_number) {
  case SIGSEGV:
    signal_name = "SIGSEGV (Segmentation fault)";
    break;
  case SIGBUS:
    signal_name = "SIGBUS (Bus error)";
    break;
  case SIGABRT:
    signal_name = "SIGABRT (Abort)";
    break;
  case SIGILL:
    signal_name = "SIGILL (Illegal instruction)";
    break;
  case SIGFPE:
    signal_name = "SIGFPE (Floating point exception)";
    break;
  case SIGTRAP:
    signal_name = "SIGTRAP (Trace trap)";
    break;
  }

  fprintf(crash_file, "===== CRASH REPORT =====\n");
  fprintf(crash_file, "Time: %s\n", time_buffer);
  fprintf(crash_file, "Signal: %s\n", signal_name);
  fprintf(crash_file, "OS: %s %s\n", sys_info.sysname, sys_info.release);
  fprintf(crash_file, "Architecture: %s\n", sys_info.machine);
  fprintf(crash_file, "Process ID: %d\n", getpid());
  fprintf(crash_file, "\n");

  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count = backtrace(stack_frames, MAX_STACK_FRAMES);

  fprintf(crash_file, "To symbolicate this crash, you can use:\n");

  Dl_info first_frame_info;
  if (frame_count > 0 && dladdr(stack_frames[0], &first_frame_info) &&
      first_frame_info.dli_fname) {
    fprintf(crash_file, "atos -o %s -l %p", first_frame_info.dli_fname,
            first_frame_info.dli_fbase);
    for (int i = 0; i < frame_count; i++) {
      fprintf(crash_file, " %p", stack_frames[i]);
    }
    fprintf(crash_file, "\n\n");
  }

  capture_and_print_stacktrace(crash_file, 0);

  if (crash_file != stderr) {
    fclose(crash_file);
  }
}

static void crash_signal_handler(int signal_number) {
  write_crash_info_to_file(signal_number);

#ifdef DEBUG
  const char *crash_log_path = get_crash_log_path();
  FILE *crash_file = fopen(crash_log_path, "r");
  if (crash_file) {
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "CRASH DETECTED\n");
    fprintf(stderr, "========================================\n");

    char line[512];
    while (fgets(line, sizeof(line), crash_file)) {
      fprintf(stderr, "%s", line);
    }

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    fclose(crash_file);
  }
#endif

  fprintf(stderr, "\nCrash log written to: %s\n", get_crash_log_path());

  signal(signal_number, SIG_DFL);
  raise(signal_number);
}

static void check_previous_crash(void) {
  // todo: this is for uploading the crash
  // const char* crash_log_path = get_crash_log_path();
  // struct stat st;
  // if (stat(crash_log_path, &st) != 0) {
  //   return;
  // }
  //
  // FILE* crash_file = fopen(crash_log_path, "r");
  // if (!crash_file) {
  //   return;
  // }
  //
  // fprintf(stderr, "\n");
  // fprintf(stderr, "========================================\n");
  // fprintf(stderr, "PREVIOUS CRASH DETECTED\n");
  // fprintf(stderr, "========================================\n");
  //
  // char line[512];
  // while (fgets(line, sizeof(line), crash_file)) {
  //   fprintf(stderr, "%s", line);
  // }
  //
  // fprintf(stderr, "========================================\n");
  // fprintf(stderr, "\n");
  //
  // fclose(crash_file);
  // remove(crash_log_path);
}

void os_install_crash_handler(void) {
  check_previous_crash();

  struct sigaction sa;
  sa.sa_handler = crash_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGTRAP, &sa, NULL);
}

// HTTP functions (stub implementations)
typedef int32 PlatformHttpRequestOp;
typedef int32 PlatformHttpStreamOp;

PlatformHttpRequestOp os_start_http_request(HttpMethod method, const char *url,
                                            int url_len, const char *headers,
                                            int headers_len, const char *body,
                                            int body_len) {
  UNUSED(method);
  UNUSED(url);
  UNUSED(url_len);
  UNUSED(headers);
  UNUSED(headers_len);
  UNUSED(body);
  UNUSED(body_len);
  return -1;
}

HttpOpState os_check_http_request(PlatformHttpRequestOp op_id) {
  UNUSED(op_id);
  return HTTP_OP_ERROR;
}

int32 os_get_http_response_info(PlatformHttpRequestOp op_id,
                                _out_ int32 *status_code,
                                _out_ int32 *headers_len,
                                _out_ int32 *body_len) {
  UNUSED(op_id);
  UNUSED(status_code);
  UNUSED(headers_len);
  UNUSED(body_len);
  return -1;
}

int32 os_get_http_headers(PlatformHttpRequestOp op_id, char *buffer,
                          int32 buffer_len) {
  UNUSED(op_id);
  UNUSED(buffer);
  UNUSED(buffer_len);
  return -1;
}

int32 os_get_http_body(PlatformHttpRequestOp op_id, char *buffer,
                       int32 buffer_len) {
  UNUSED(op_id);
  UNUSED(buffer);
  UNUSED(buffer_len);
  return -1;
}

PlatformHttpStreamOp os_start_http_stream(HttpMethod method, const char *url,
                                          int url_len, const char *headers,
                                          int headers_len, const char *body,
                                          int body_len) {
  UNUSED(method);
  UNUSED(url);
  UNUSED(url_len);
  UNUSED(headers);
  UNUSED(headers_len);
  UNUSED(body);
  UNUSED(body_len);
  return -1;
}

HttpStreamState os_check_http_stream(PlatformHttpStreamOp op_id) {
  UNUSED(op_id);
  return HTTP_STREAM_ERROR;
}

int32 os_get_http_stream_info(PlatformHttpStreamOp op_id,
                              _out_ int32 *status_code) {
  UNUSED(op_id);
  UNUSED(status_code);
  return -1;
}

int32 os_get_http_stream_chunk_size(PlatformHttpStreamOp op_id) {
  UNUSED(op_id);
  return -1;
}

int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                               int32 buffer_len, _out_ bool32 *is_final) {
  UNUSED(op_id);
  UNUSED(buffer);
  UNUSED(buffer_len);
  UNUSED(is_final);
  return -1;
}

#define internal static
