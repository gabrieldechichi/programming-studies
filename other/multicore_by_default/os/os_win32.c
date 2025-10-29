#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <windows.h>
#include <errhandlingapi.h>
#include <process.h>
#include <io.h>
#include <direct.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <intrin.h>
#include <time.h>
#include <string.h>

#include "os.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define isatty _isatty
#define fileno _fileno
#define mkdir _mkdir
#define popen _popen
#define pclose _pclose
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

b32 os_is_mobile() { return false; }
void os_lock_mouse(b32 lock) { UNUSED(lock); }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

struct OsThread {
  HANDLE handle;
  OsThreadFunc func;
  void *arg;
};

struct OsMutex {
  CRITICAL_SECTION cs;
};

static unsigned __stdcall thread_wrapper(void *arg) {
  OsThread *thread = (OsThread *)arg;
  thread->func(thread->arg);
  return 0;
}

OsThread *os_thread_create(OsThreadFunc func, void *arg) {
  OsThread *thread = (OsThread *)malloc(sizeof(OsThread));
  if (!thread)
    return NULL;

  thread->func = func;
  thread->arg = arg;
  thread->handle =
      (HANDLE)_beginthreadex(NULL, 0, thread_wrapper, thread, 0, NULL);

  if (thread->handle == 0) {
    free(thread);
    return NULL;
  }

  return thread;
}

void os_thread_join(OsThread *thread) {
  if (thread && thread->handle) {
    WaitForSingleObject(thread->handle, INFINITE);
  }
}

void os_thread_destroy(OsThread *thread) {
  if (thread) {
    if (thread->handle) {
      CloseHandle(thread->handle);
    }
    free(thread);
  }
}

OsMutex *os_mutex_create(void) {
  OsMutex *mutex = (OsMutex *)malloc(sizeof(OsMutex));
  if (!mutex)
    return NULL;

  InitializeCriticalSection(&mutex->cs);
  return mutex;
}

void os_mutex_lock(OsMutex *mutex) {
  if (mutex) {
    EnterCriticalSection(&mutex->cs);
  }
}

void os_mutex_unlock(OsMutex *mutex) {
  if (mutex) {
    LeaveCriticalSection(&mutex->cs);
  }
}

void os_mutex_destroy(OsMutex *mutex) {
  if (mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
  }
}

#define WORK_QUEUE_ENTRIES_MAX 256

typedef struct {
  OsWorkQueueCallback callback;
  void *data;
} WorkQueueEntry;

struct OsWorkQueue {
  WorkQueueEntry entries[WORK_QUEUE_ENTRIES_MAX];

  volatile LONG next_entry_to_write;
  volatile LONG next_entry_to_read;

  volatile LONG completion_goal;
  volatile LONG completion_count;

  HANDLE semaphore;
  HANDLE *worker_threads;
  i32 thread_count;
  volatile LONG should_quit;
};

static DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
  OsWorkQueue *queue = (OsWorkQueue *)lpParam;

  while (!queue->should_quit) {
    LONG original_next_read = queue->next_entry_to_read;
    LONG new_next_read = (original_next_read + 1) % WORK_QUEUE_ENTRIES_MAX;

    if (original_next_read != queue->next_entry_to_write) {
      LONG index = InterlockedCompareExchange(
          &queue->next_entry_to_read, new_next_read, original_next_read);

      if (index == original_next_read) {
        WorkQueueEntry entry = queue->entries[index];
        entry.callback(entry.data);
        InterlockedIncrement(&queue->completion_count);
      }
    } else {
      WaitForSingleObject(queue->semaphore, INFINITE);
    }
  }

  return 0;
}

OsWorkQueue *os_work_queue_create(i32 thread_count) {
  OsWorkQueue *queue = (OsWorkQueue *)malloc(sizeof(OsWorkQueue));
  if (!queue) {
    return NULL;
  }

  memset(queue, 0, sizeof(OsWorkQueue));

  queue->thread_count = thread_count;
  queue->semaphore = CreateSemaphoreA(NULL, 0, thread_count, NULL);
  if (!queue->semaphore) {
    free(queue);
    return NULL;
  }

  queue->worker_threads = (HANDLE *)malloc(sizeof(HANDLE) * thread_count);
  if (!queue->worker_threads) {
    CloseHandle(queue->semaphore);
    free(queue);
    return NULL;
  }

  for (i32 i = 0; i < thread_count; i++) {
    queue->worker_threads[i] =
        CreateThread(NULL, 0, WorkerThreadProc, queue, 0, NULL);
    if (!queue->worker_threads[i]) {
      queue->should_quit = 1;
      for (i32 j = 0; j < i; j++) {
        WaitForSingleObject(queue->worker_threads[j], INFINITE);
        CloseHandle(queue->worker_threads[j]);
      }
      free(queue->worker_threads);
      CloseHandle(queue->semaphore);
      free(queue);
      return NULL;
    }
  }

  return queue;
}

void os_work_queue_destroy(OsWorkQueue *queue) {
  if (!queue) {
    return;
  }

  queue->should_quit = 1;

  ReleaseSemaphore(queue->semaphore, queue->thread_count, NULL);

  for (i32 i = 0; i < queue->thread_count; i++) {
    WaitForSingleObject(queue->worker_threads[i], INFINITE);
    CloseHandle(queue->worker_threads[i]);
  }

  free(queue->worker_threads);
  CloseHandle(queue->semaphore);
  free(queue);
}

void os_add_work_entry(OsWorkQueue *queue, OsWorkQueueCallback callback,
                       void *data) {
  LONG new_next_write =
      (queue->next_entry_to_write + 1) % WORK_QUEUE_ENTRIES_MAX;
  assert_msg(new_next_write != queue->next_entry_to_read,
             "Work queue is full!");

  WorkQueueEntry *entry = &queue->entries[queue->next_entry_to_write];
  entry->callback = callback;
  entry->data = data;

  InterlockedIncrement(&queue->completion_goal);

  _WriteBarrier();

  queue->next_entry_to_write = new_next_write;

  ReleaseSemaphore(queue->semaphore, 1, NULL);
}

void os_complete_all_work(OsWorkQueue *queue) {
  while (queue->completion_count != queue->completion_goal) {
    LONG original_next_read = queue->next_entry_to_read;
    LONG new_next_read = (original_next_read + 1) % WORK_QUEUE_ENTRIES_MAX;

    if (original_next_read != queue->next_entry_to_write) {
      LONG index = InterlockedCompareExchange(
          &queue->next_entry_to_read, new_next_read, original_next_read);

      if (index == original_next_read) {
        WorkQueueEntry entry = queue->entries[index];
        entry.callback(entry.data);
        InterlockedIncrement(&queue->completion_count);
      }
    }
  }

  queue->completion_goal = 0;
  queue->completion_count = 0;
}

#define MAX_STACK_FRAMES 50
#define MAX_SYMBOL_LEN 512
#define CRASH_DUMP_DIR "crashes"

#pragma comment(lib, "dbghelp.lib")

static OsMutex *g_stack_trace_mutex = NULL;
static b32 g_symbols_initialized = false;
static LPTOP_LEVEL_EXCEPTION_FILTER g_previous_exception_filter = NULL;

static void ensure_symbols_initialized(void) {
  if (!g_symbols_initialized) {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (SymInitialize(process, NULL, TRUE)) {
      g_symbols_initialized = true;
    }
  }
}

static void ensure_crash_dir_exists(void) { os_create_dir(CRASH_DUMP_DIR); }

static void write_stack_to_buffer(char *buffer, size_t buffer_size,
                                  void **stack_frames, int frame_count,
                                  int skip_frames) {
  HANDLE process = GetCurrentProcess();
  size_t written = 0;

  written += snprintf(buffer + written, buffer_size - written,
                      "\n===== STACK TRACE =====\n");

  for (int i = skip_frames; i < frame_count && written < buffer_size - 1; i++) {
    char line_buffer[1024];
    int line_len = 0;

    line_len += snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len,
                         "  [%2d] ", i - skip_frames);

    HMODULE module;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(stack_frames[i], &mbi, sizeof(mbi)) &&
        mbi.AllocationBase) {
      module = (HMODULE)mbi.AllocationBase;

      char module_name[MAX_PATH];
      if (GetModuleFileNameA(module, module_name, sizeof(module_name))) {
        char *base_name = strrchr(module_name, '\\');
        if (base_name)
          base_name++;
        else
          base_name = module_name;

        if (g_symbols_initialized) {
          DWORD64 displacement64 = 0;
          char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYMBOL_LEN];
          SYMBOL_INFO *symbol = (SYMBOL_INFO *)symbol_buffer;
          symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
          symbol->MaxNameLen = MAX_SYMBOL_LEN;

          if (SymFromAddr(process, (DWORD64)stack_frames[i], &displacement64,
                          symbol)) {
            line_len += snprintf(line_buffer + line_len,
                                 sizeof(line_buffer) - line_len, "%s!%s+0x%llx",
                                 base_name, symbol->Name, displacement64);

            DWORD displacement32;
            IMAGEHLP_LINE64 line_info;
            line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

            if (SymGetLineFromAddr64(process, (DWORD64)stack_frames[i],
                                     &displacement32, &line_info)) {
              char *source_name = strrchr(line_info.FileName, '\\');
              if (source_name)
                source_name++;
              else
                source_name = line_info.FileName;

              line_len += snprintf(line_buffer + line_len,
                                   sizeof(line_buffer) - line_len, " (%s:%lu)",
                                   source_name, line_info.LineNumber);
            }
          } else {
            line_len +=
                snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len,
                         "%s+0x%llx", base_name,
                         (DWORD64)stack_frames[i] - (DWORD64)module);
          }
        } else {
          line_len +=
              snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len,
                       "%s+0x%llx", base_name,
                       (DWORD64)stack_frames[i] - (DWORD64)module);
        }
      } else {
        line_len +=
            snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len,
                     "0x%p", stack_frames[i]);
      }
    } else {
      line_len +=
          snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len,
                   "0x%p", stack_frames[i]);
    }

    line_len +=
        snprintf(line_buffer + line_len, sizeof(line_buffer) - line_len, "\n");

    if (written + line_len < buffer_size) {
      memcpy(buffer + written, line_buffer, line_len);
      written += line_len;
    }
  }

  written += snprintf(buffer + written, buffer_size - written,
                      "=======================\n");
  buffer[buffer_size - 1] = '\0';
}

static void capture_and_save_stacktrace(FILE *output, int skip_frames) {
  if (!g_stack_trace_mutex) {
    g_stack_trace_mutex = os_mutex_create();
  }

  os_mutex_lock(g_stack_trace_mutex);

  ensure_symbols_initialized();

  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count =
      CaptureStackBackTrace(0, MAX_STACK_FRAMES, stack_frames, NULL);

  if (frame_count <= skip_frames) {
    os_mutex_unlock(g_stack_trace_mutex);
    return;
  }

  char *stack_buffer = malloc(64 * 1024);
  if (!stack_buffer) {
    fprintf(output, "\n===== STACK TRACE (allocation failed) =====\n");
    for (int i = skip_frames; i < frame_count; i++) {
      fprintf(output, "  [%2d] 0x%p\n", i - skip_frames, stack_frames[i]);
    }
    fprintf(output, "=======================\n");
    os_mutex_unlock(g_stack_trace_mutex);
    return;
  }

  write_stack_to_buffer(stack_buffer, 64 * 1024, stack_frames, frame_count,
                        skip_frames);

  fprintf(output, "%s", stack_buffer);
  fflush(output);

  ensure_crash_dir_exists();

  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);

  char crash_filename[256];
  snprintf(crash_filename, sizeof(crash_filename),
           "%s/crash_%04d%02d%02d_%02d%02d%02d.txt", CRASH_DUMP_DIR,
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  FILE *crash_file = fopen(crash_filename, "w");
  if (crash_file) {
    fprintf(crash_file, "Crash dump generated at %s", asctime(timeinfo));
    fprintf(crash_file, "Symbol resolution: %s\n",
            g_symbols_initialized ? "Available"
                                  : "Not available (PDB files may be missing)");
    fprintf(crash_file, "%s", stack_buffer);

    if (!g_symbols_initialized) {
      fprintf(crash_file, "\nNote: To get function names and line numbers, "
                          "ensure PDB files are available.\n");
      fprintf(crash_file, "Raw addresses can be resolved later using:\n");
      fprintf(crash_file, "  - Visual Studio debugger\n");
      fprintf(crash_file, "  - WinDbg\n");
      fprintf(crash_file, "  - addr2line or similar tools\n");
    }

    fclose(crash_file);
    fprintf(output, "Stack trace saved to: %s\n", crash_filename);
  }

  free(stack_buffer);
  os_mutex_unlock(g_stack_trace_mutex);
}

static const char* get_exception_string(DWORD exception_code) {
  switch (exception_code) {
    case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:          return "FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:            return "FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
    default:                                 return "UNKNOWN_EXCEPTION";
  }
}

static LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS* exception_info) {
  if (!g_stack_trace_mutex) {
    g_stack_trace_mutex = os_mutex_create();
  }

  os_mutex_lock(g_stack_trace_mutex);

  ensure_symbols_initialized();
  ensure_crash_dir_exists();

  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);

  char crash_filename[256];
  snprintf(crash_filename, sizeof(crash_filename),
           "%s/crash_%04d%02d%02d_%02d%02d%02d.txt", CRASH_DUMP_DIR,
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  FILE *crash_file = fopen(crash_filename, "w");
  FILE *console_output = stderr;

  PEXCEPTION_RECORD exception_record = exception_info->ExceptionRecord;
  PCONTEXT context = exception_info->ContextRecord;

  const char *exception_str = get_exception_string(exception_record->ExceptionCode);

  char header[1024];
  snprintf(header, sizeof(header),
           "\n===== FATAL EXCEPTION =====\n"
           "Exception: %s (0x%08lX)\n"
           "Address: 0x%p\n",
           exception_str,
           exception_record->ExceptionCode,
           exception_record->ExceptionAddress);

  if (exception_record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    const char *access_type = (exception_record->ExceptionInformation[0] == 0) ? "reading" :
                              (exception_record->ExceptionInformation[0] == 1) ? "writing" :
                              "executing";
    char access_info[256];
    snprintf(access_info, sizeof(access_info),
             "Access violation %s address: 0x%p\n",
             access_type,
             (void*)exception_record->ExceptionInformation[1]);
    strcat(header, access_info);
  }

  fprintf(console_output, "%s", header);
  if (crash_file) {
    fprintf(crash_file, "Crash dump generated at %s", asctime(timeinfo));
    fprintf(crash_file, "%s", header);
  }

  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count = 0;

  #ifdef _M_X64
  STACKFRAME64 stack_frame;
  memset(&stack_frame, 0, sizeof(stack_frame));

  DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
  stack_frame.AddrPC.Offset = context->Rip;
  stack_frame.AddrPC.Mode = AddrModeFlat;
  stack_frame.AddrFrame.Offset = context->Rbp;
  stack_frame.AddrFrame.Mode = AddrModeFlat;
  stack_frame.AddrStack.Offset = context->Rsp;
  stack_frame.AddrStack.Mode = AddrModeFlat;
  #else
  STACKFRAME64 stack_frame;
  memset(&stack_frame, 0, sizeof(stack_frame));

  DWORD machine_type = IMAGE_FILE_MACHINE_I386;
  stack_frame.AddrPC.Offset = context->Eip;
  stack_frame.AddrPC.Mode = AddrModeFlat;
  stack_frame.AddrFrame.Offset = context->Ebp;
  stack_frame.AddrFrame.Mode = AddrModeFlat;
  stack_frame.AddrStack.Offset = context->Esp;
  stack_frame.AddrStack.Mode = AddrModeFlat;
  #endif

  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();

  while (frame_count < MAX_STACK_FRAMES) {
    if (!StackWalk64(machine_type, process, thread, &stack_frame, context,
                     NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
      break;
    }

    if (stack_frame.AddrPC.Offset == 0) {
      break;
    }

    stack_frames[frame_count++] = (void*)(uintptr_t)stack_frame.AddrPC.Offset;
  }

  char *stack_buffer = malloc(64 * 1024);
  if (stack_buffer) {
    write_stack_to_buffer(stack_buffer, 64 * 1024, stack_frames, frame_count, 0);
    fprintf(console_output, "%s", stack_buffer);
    if (crash_file) {
      fprintf(crash_file, "%s", stack_buffer);
    }
    free(stack_buffer);
  }

  if (crash_file) {
    fprintf(crash_file, "\nRegisters:\n");
    #ifdef _M_X64
    fprintf(crash_file, "RAX=%016llX RBX=%016llX RCX=%016llX\n",
            context->Rax, context->Rbx, context->Rcx);
    fprintf(crash_file, "RDX=%016llX RSI=%016llX RDI=%016llX\n",
            context->Rdx, context->Rsi, context->Rdi);
    fprintf(crash_file, "RIP=%016llX RSP=%016llX RBP=%016llX\n",
            context->Rip, context->Rsp, context->Rbp);
    fprintf(crash_file, "R8 =%016llX R9 =%016llX R10=%016llX\n",
            context->R8, context->R9, context->R10);
    fprintf(crash_file, "R11=%016llX R12=%016llX R13=%016llX\n",
            context->R11, context->R12, context->R13);
    fprintf(crash_file, "R14=%016llX R15=%016llX\n",
            context->R14, context->R15);
    #else
    fprintf(crash_file, "EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n",
            context->Eax, context->Ebx, context->Ecx, context->Edx);
    fprintf(crash_file, "ESI=%08X EDI=%08X EIP=%08X ESP=%08X\n",
            context->Esi, context->Edi, context->Eip, context->Esp);
    fprintf(crash_file, "EBP=%08X EFL=%08X\n",
            context->Ebp, context->EFlags);
    #endif

    fclose(crash_file);
    fprintf(console_output, "\nCrash dump saved to: %s\n", crash_filename);
  }

  fprintf(console_output, "===========================\n");
  fflush(console_output);

  os_mutex_unlock(g_stack_trace_mutex);

  if (g_previous_exception_filter) {
    return g_previous_exception_filter(exception_info);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

void os_install_crash_handler(void) {
  g_previous_exception_filter = SetUnhandledExceptionFilter(unhandled_exception_handler);

  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
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

  if (log_level == LOGLEVEL_ERROR) {
    capture_and_save_stacktrace(output, 2);
  }

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
  struct stat st;
  if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  char temp_path[MAX_PATH];
  strncpy(temp_path, dir_path, sizeof(temp_path) - 1);
  temp_path[sizeof(temp_path) - 1] = '\0';

  for (char *p = temp_path + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      *p = '\0';

      if (stat(temp_path, &st) != 0) {
        if (_mkdir(temp_path) != 0) {
          LOG_ERROR("Failed to create directory: %", FMT_STR(temp_path));
          return false;
        }
      }

      *p = '\\';
    }
  }

  if (_mkdir(temp_path) != 0) {
    if (stat(temp_path, &st) == 0 && S_ISDIR(st.st_mode)) {
      return true;
    }
    LOG_ERROR("Failed to create directory: %", FMT_STR(dir_path));
    return false;
  }

  return true;
}

PlatformFileData os_read_file(const char *file_path, Allocator *allocator) {
  PlatformFileData result = {0};

  FILE *file = fopen(file_path, "rb");
  if (!file) {
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
    LOG_ERROR("Failed to read file completely: %", FMT_STR(file_path));
    return result;
  }

  result.buffer_len = (uint32)file_size;
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

OsDynLib os_dynlib_load(const char *path) {
  OsDynLib lib = LoadLibraryA(path);
  if (!lib) {
    DWORD err = GetLastError();
    LOG_ERROR("os_dynlib_load failed. Error code %", FMT_UINT(err));
  }
  return lib;
}

void os_dynlib_unload(OsDynLib lib) {
  if (lib) {
    FreeLibrary((HMODULE)lib);
  }
}

OsDynSymbol os_dynlib_get_symbol(OsDynLib lib, const char *symbol_name) {
  if (!lib)
    return NULL;
  return (OsDynSymbol)GetProcAddress((HMODULE)lib, symbol_name);
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

b32 os_file_copy(const char *src_path, const char *dst_path) {
  return CopyFileA(src_path, dst_path, FALSE) != 0;
}

b32 os_file_remove(const char *path) { return DeleteFileA(path) != 0; }

b32 os_file_exists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return attrs != INVALID_FILE_ATTRIBUTES;
}

static b32 copy_directory_recursive(const char *src_path,
                                    const char *dst_path) {
  if (!os_create_dir(dst_path)) {
    return false;
  }

  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*", src_path);

  WIN32_FIND_DATAA find_data;
  HANDLE find_handle = FindFirstFileA(search_path, &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  b32 success = true;
  do {
    if (strcmp(find_data.cFileName, ".") == 0 ||
        strcmp(find_data.cFileName, "..") == 0) {
      continue;
    }

    char src_full[MAX_PATH];
    char dst_full[MAX_PATH];
    snprintf(src_full, sizeof(src_full), "%s\\%s", src_path,
             find_data.cFileName);
    snprintf(dst_full, sizeof(dst_full), "%s\\%s", dst_path,
             find_data.cFileName);

    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (!copy_directory_recursive(src_full, dst_full)) {
        success = false;
        break;
      }
    } else {
      if (!os_file_copy(src_full, dst_full)) {
        success = false;
        break;
      }
    }
  } while (FindNextFileA(find_handle, &find_data));

  FindClose(find_handle);
  return success;
}

b32 os_directory_copy(const char *src_path, const char *dst_path) {
  return copy_directory_recursive(src_path, dst_path);
}

static b32 remove_directory_recursive(const char *path) {
  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*", path);

  WIN32_FIND_DATAA find_data;
  HANDLE find_handle = FindFirstFileA(search_path, &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  b32 success = true;
  do {
    if (strcmp(find_data.cFileName, ".") == 0 ||
        strcmp(find_data.cFileName, "..") == 0) {
      continue;
    }

    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName);

    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (!remove_directory_recursive(full_path)) {
        success = false;
        break;
      }
    } else {
      if (!DeleteFileA(full_path)) {
        success = false;
        break;
      }
    }
  } while (FindNextFileA(find_handle, &find_data));

  FindClose(find_handle);

  if (success) {
    return RemoveDirectoryA(path) != 0;
  }
  return false;
}

b32 os_directory_remove(const char *path) {
  return remove_directory_recursive(path);
}

b32 os_system(const char *command) { return system(command) == 0; }

OsFileList os_list_files(const char *directory, const char *extension,
                         Allocator *allocator) {
  OsFileList result = {0};

  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*%s", directory, extension);

  WIN32_FIND_DATAA find_data;
  HANDLE find_handle = FindFirstFileA(search_path, &find_data);

  if (find_handle == INVALID_HANDLE_VALUE) {
    return result;
  }

  int count = 0;
  int capacity = 256;
  char **paths = ALLOC_ARRAY(allocator, char *, capacity);

  do {
    if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (count < capacity) {
        size_t path_len = strlen(directory) + strlen(find_data.cFileName) + 2;
        char *full_path =
            allocator->alloc_alloc(allocator->ctx, path_len, 1);
        if (full_path) {
          snprintf(full_path, path_len, "%s/%s", directory,
                   find_data.cFileName);
          paths[count++] = full_path;
        }
      }
    }
  } while (FindNextFileA(find_handle, &find_data) && count < capacity);

  FindClose(find_handle);

  result.paths = paths;
  result.count = count;
  return result;
}

b32 os_file_set_executable(const char *path) {
  UNUSED(path);
  return true;
}

char *os_cwd(char *buffer, u32 buffer_size) {
  if (!_getcwd(buffer, buffer_size)) {
    return NULL;
  }
  for (u32 i = 0; i < buffer_size && buffer[i] != '\0'; i++) {
    if (buffer[i] == '\\') {
      buffer[i] = '/';
    }
  }
  return buffer;
}

static struct {
  LARGE_INTEGER freq;
  LARGE_INTEGER start;
  b32 initialized;
} g_time_state = {0};

static int64_t _stm_int64_muldiv(int64_t value, int64_t numer, int64_t denom) {
  int64_t q = value / denom;
  int64_t r = value % denom;
  return q * numer + r * numer / denom;
}

void os_time_init(void) {
  QueryPerformanceFrequency(&g_time_state.freq);
  QueryPerformanceCounter(&g_time_state.start);
  g_time_state.initialized = true;
}

u64 os_time_now(void) {
  assert(g_time_state.initialized);
  LARGE_INTEGER qpc_t;
  QueryPerformanceCounter(&qpc_t);
  u64 now = (u64)_stm_int64_muldiv(qpc_t.QuadPart - g_time_state.start.QuadPart,
                                   1000000000, g_time_state.freq.QuadPart);
  return now;
}

u64 os_time_diff(u64 new_ticks, u64 old_ticks) {
  if (new_ticks > old_ticks) {
    return new_ticks - old_ticks;
  } else {
    return 1;
  }
}

f64 os_ticks_to_ms(u64 ticks) { return (f64)ticks / 1000000.0; }

f64 os_ticks_to_us(u64 ticks) { return (f64)ticks / 1000.0; }

f64 os_ticks_to_ns(u64 ticks) { return (f64)ticks; }

i32 os_get_processor_count(void) {
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return (i32)sysinfo.dwNumberOfProcessors;
}

u8 *os_allocate_memory(size_t size) {
  void *memory =
      VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!memory) {
    DWORD err = GetLastError();
    LOG_ERROR("VirtualAlloc failed. Size: %, Error: %", FMT_UINT(size),
              FMT_UINT(err));
    return NULL;
  }
  return memory;
}

void os_free_memory(void *ptr, size_t size) {
  UNUSED(size);
  if (ptr) {
    if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
      DWORD err = GetLastError();
      LOG_ERROR("VirtualFree failed. Error: %", FMT_UINT(err));
    }
  }
}

OsWebPLoadOp os_start_webp_texture_load(const char *file_path,
                                         u32 file_path_len,
                                         u32 texture_handle_idx,
                                         u32 texture_handle_gen) {
  UNUSED(file_path);
  UNUSED(file_path_len);
  UNUSED(texture_handle_idx);
  UNUSED(texture_handle_gen);
  return -1;
}

OsFileReadState os_check_webp_texture_load(OsWebPLoadOp op_id) {
  UNUSED(op_id);
  return OS_FILE_READ_STATE_ERROR;
}
