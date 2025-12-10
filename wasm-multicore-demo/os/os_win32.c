#include "lib/typedefs.h"
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
#include <winternl.h>
#include <errhandlingapi.h>
#include <process.h>
#include <io.h>
#include <direct.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <intrin.h>

#include "lib/thread.h"
#include "lib/task.h"
#include "lib/string_builder.h"
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

#ifndef internal
#define internal static
#endif

typedef enum {
  OS_W32_ENTITY_NULL,
  OS_W32_ENTITY_THREAD,
  OS_W32_ENTITY_MUTEX,
  OS_W32_ENTITY_SEMAPHORE,
  OS_W32_ENTITY_FILE_OP,
} OsWin32EntityKind;

typedef struct OsWin32Entity OsWin32Entity;
struct OsWin32Entity {
  OsWin32Entity *next;
  OsWin32EntityKind kind;
  union {
    struct {
      HANDLE handle;
      ThreadFunc func;
      void *arg;
    } thread;
    CRITICAL_SECTION mutex;
    struct {
      CRITICAL_SECTION cs;
      CONDITION_VARIABLE cv;
      i32 count;
    } semaphore;
    struct {
      volatile OsFileReadState state;
      char file_path[MAX_PATH];
      u8 *buffer;
      u32 buffer_len;
    } file_op;
  };
};

#define OS_W32_ENTITY_POOL_SIZE 256
#define OS_W32_ENTITY_POOL_MEMORY_SIZE                                         \
  (sizeof(OsWin32Entity) * OS_W32_ENTITY_POOL_SIZE)

typedef struct {
    union { LONG Status; PVOID Pointer; };
    ULONG_PTR Information;
} NT_IO_STATUS_BLOCK;

typedef struct {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    WCHAR FileName[1];
} NT_FILE_DIRECTORY_INFORMATION;

typedef LONG (NTAPI *PFN_NtQueryDirectoryFile)(
    HANDLE FileHandle,
    HANDLE Event,
    PVOID ApcRoutine,
    PVOID ApcContext,
    NT_IO_STATUS_BLOCK *IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    INT FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    PVOID FileName,
    BOOLEAN RestartScan
);

typedef LONG(NTAPI *PFN_NtDelayExecution)(BOOLEAN Alertable,
                                          PLARGE_INTEGER DelayInterval);

typedef struct {
  b32 initialized;

  LARGE_INTEGER time_freq;
  LARGE_INTEGER time_start;
  f64 time_freq_inv_ns;

  u32 processor_count;
  u32 page_size;

  CRITICAL_SECTION entity_mutex;
  u8 entity_memory[OS_W32_ENTITY_POOL_MEMORY_SIZE];
  PoolAllocator entity_pool;
  OsWin32Entity *entity_free;

  u8 stack_trace_buffer[KB(64)];

  PFN_NtQueryDirectoryFile NtQueryDirectoryFile;
  PFN_NtDelayExecution NtDelayExecution;
} OsWin32State;

global OsWin32State os_w32_state = {0};


#define os_w32_assert_state_initialized() debug_assert(os_w32_state.initialized)

internal OsWin32Entity *os_w32_entity_alloc(OsWin32EntityKind kind) {
  os_w32_assert_state_initialized();
  OsWin32Entity *result = NULL;
  EnterCriticalSection(&os_w32_state.entity_mutex);
  {
    if (os_w32_state.entity_free) {
      result = os_w32_state.entity_free;
      os_w32_state.entity_free = result->next;
    } else {
      result = (OsWin32Entity *)pool_alloc(&os_w32_state.entity_pool);
    }
    if (result) {
      memset(result, 0, sizeof(OsWin32Entity));
    }
  }
  LeaveCriticalSection(&os_w32_state.entity_mutex);
  if (result) {
    result->kind = kind;
  }
  return result;
}

internal void os_w32_entity_release(OsWin32Entity *entity) {
  if (!entity)
    return;
  os_w32_assert_state_initialized();
  entity->kind = OS_W32_ENTITY_NULL;
  EnterCriticalSection(&os_w32_state.entity_mutex);
  entity->next = os_w32_state.entity_free;
  os_w32_state.entity_free = entity;
  LeaveCriticalSection(&os_w32_state.entity_mutex);
}

b32 os_is_mobile() { return false; }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

internal unsigned __stdcall os_w32_thread_wrapper(void *arg) {
  OsWin32Entity *entity = (OsWin32Entity *)arg;
  entity->thread.func(entity->thread.arg);
  return 0;
}

void os_init(void) {
  if (os_w32_state.initialized)
    return;

  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  os_w32_state.processor_count = sysinfo.dwNumberOfProcessors;
  os_w32_state.page_size = sysinfo.dwPageSize;

  QueryPerformanceFrequency(&os_w32_state.time_freq);
  QueryPerformanceCounter(&os_w32_state.time_start);
  os_w32_state.time_freq_inv_ns = 1000000000.0 / (f64)os_w32_state.time_freq.QuadPart;

  InitializeCriticalSection(&os_w32_state.entity_mutex);
  os_w32_state.entity_pool =
      pool_from_buffer(os_w32_state.entity_memory,
                       OS_W32_ENTITY_POOL_MEMORY_SIZE, sizeof(OsWin32Entity));
  os_w32_state.entity_free = NULL;

  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  if (ntdll) {
    os_w32_state.NtQueryDirectoryFile = (PFN_NtQueryDirectoryFile)GetProcAddress(ntdll, "NtQueryDirectoryFile");
    os_w32_state.NtDelayExecution = (PFN_NtDelayExecution)GetProcAddress(ntdll, "NtDelayExecution");
  }

  os_w32_state.initialized = true;
}

Thread os_thread_launch(ThreadFunc func, void *arg) {
  Thread result = {0};
  OsWin32Entity *entity = os_w32_entity_alloc(OS_W32_ENTITY_THREAD);
  if (!entity)
    return result;

  entity->thread.func = func;
  entity->thread.arg = arg;
  entity->thread.handle =
      (HANDLE)_beginthreadex(NULL, 0, os_w32_thread_wrapper, entity, 0, NULL);

  if (entity->thread.handle == 0) {
    os_w32_entity_release(entity);
    return result;
  }

  result.v[0] = (u64)entity;
  return result;
}

b32 os_thread_join(Thread t, u64 timeout_us) {
  if (t.v[0] == 0)
    return false;
  OsWin32Entity *entity = (OsWin32Entity *)t.v[0];
  if (entity->thread.handle) {
    DWORD timeout_ms =
        (timeout_us == 0) ? INFINITE : (DWORD)(timeout_us / 1000);
    DWORD result = WaitForSingleObject(entity->thread.handle, timeout_ms);
    if (result == WAIT_OBJECT_0) {
      CloseHandle(entity->thread.handle);
      os_w32_entity_release(entity);
      return true;
    }
    return false;
  }
  return false;
}

void os_thread_detach(Thread t) {
  if (t.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)t.v[0];
  if (entity->thread.handle) {
    CloseHandle(entity->thread.handle);
  }
  os_w32_entity_release(entity);
}

void os_thread_set_name(Thread t, const char *name) {
  if (t.v[0] == 0 || !name)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)t.v[0];
  if (entity->thread.handle) {
    wchar_t wname[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 256);
    SetThreadDescription(entity->thread.handle, wname);
  }
}

Mutex os_mutex_alloc(void) {
  Mutex result = {0};
  OsWin32Entity *entity = os_w32_entity_alloc(OS_W32_ENTITY_MUTEX);
  if (!entity)
    return result;

  InitializeCriticalSection(&entity->mutex);
  result.v[0] = (u64)entity;
  return result;
}

void os_mutex_release(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)m.v[0];
  DeleteCriticalSection(&entity->mutex);
  os_w32_entity_release(entity);
}

void os_mutex_take(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)m.v[0];
  EnterCriticalSection(&entity->mutex);
}

void os_mutex_drop(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)m.v[0];
  LeaveCriticalSection(&entity->mutex);
}

Semaphore os_semaphore_alloc(i32 initial_count) {
  Semaphore result = {0};
  OsWin32Entity *entity = os_w32_entity_alloc(OS_W32_ENTITY_SEMAPHORE);
  if (!entity)
    return result;

  InitializeCriticalSection(&entity->semaphore.cs);
  InitializeConditionVariable(&entity->semaphore.cv);
  entity->semaphore.count = initial_count;
  result.v[0] = (u64)entity;
  return result;
}

void os_semaphore_release(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)s.v[0];
  DeleteCriticalSection(&entity->semaphore.cs);
  os_w32_entity_release(entity);
}

void os_semaphore_take(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)s.v[0];

  EnterCriticalSection(&entity->semaphore.cs);
  while (entity->semaphore.count <= 0) {
    SleepConditionVariableCS(&entity->semaphore.cv, &entity->semaphore.cs,
                             INFINITE);
  }
  entity->semaphore.count--;
  LeaveCriticalSection(&entity->semaphore.cs);
}

void os_semaphore_drop(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsWin32Entity *entity = (OsWin32Entity *)s.v[0];

  EnterCriticalSection(&entity->semaphore.cs);
  entity->semaphore.count++;
  LeaveCriticalSection(&entity->semaphore.cs);
  WakeConditionVariable(&entity->semaphore.cv);
}

typedef struct {
  SRWLOCK lock;
} OsWin32RWMutex;

RWMutex os_rw_mutex_alloc(void) {
  RWMutex result = {0};
  OsWin32RWMutex *rw =
      (OsWin32RWMutex *)os_allocate_memory(sizeof(OsWin32RWMutex));
  if (!rw)
    return result;
  InitializeSRWLock(&rw->lock);
  result.v[0] = (u64)rw;
  return result;
}

void os_rw_mutex_release(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32RWMutex *rw = (OsWin32RWMutex *)m.v[0];
  os_free_memory(rw, sizeof(OsWin32RWMutex));
}

void os_rw_mutex_take_r(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32RWMutex *rw = (OsWin32RWMutex *)m.v[0];
  AcquireSRWLockShared(&rw->lock);
}

void os_rw_mutex_drop_r(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32RWMutex *rw = (OsWin32RWMutex *)m.v[0];
  ReleaseSRWLockShared(&rw->lock);
}

void os_rw_mutex_take_w(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32RWMutex *rw = (OsWin32RWMutex *)m.v[0];
  AcquireSRWLockExclusive(&rw->lock);
}

void os_rw_mutex_drop_w(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsWin32RWMutex *rw = (OsWin32RWMutex *)m.v[0];
  ReleaseSRWLockExclusive(&rw->lock);
}

typedef struct {
  CONDITION_VARIABLE cv;
} OsWin32CondVar;

CondVar os_cond_var_alloc(void) {
  CondVar result = {0};
  OsWin32CondVar *cv =
      (OsWin32CondVar *)os_allocate_memory(sizeof(OsWin32CondVar));
  if (!cv)
    return result;
  InitializeConditionVariable(&cv->cv);
  result.v[0] = (u64)cv;
  return result;
}

void os_cond_var_release(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsWin32CondVar *cv = (OsWin32CondVar *)c.v[0];
  os_free_memory(cv, sizeof(OsWin32CondVar));
}

b32 os_cond_var_wait(CondVar c, Mutex m, u64 timeout_us) {
  if (c.v[0] == 0 || m.v[0] == 0)
    return false;
  OsWin32CondVar *cv = (OsWin32CondVar *)c.v[0];
  OsWin32Entity *mutex_entity = (OsWin32Entity *)m.v[0];
  DWORD timeout_ms = (timeout_us == 0) ? INFINITE : (DWORD)(timeout_us / 1000);
  return SleepConditionVariableCS(&cv->cv, &mutex_entity->mutex, timeout_ms) !=
         0;
}

void os_cond_var_signal(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsWin32CondVar *cv = (OsWin32CondVar *)c.v[0];
  WakeConditionVariable(&cv->cv);
}

void os_cond_var_broadcast(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsWin32CondVar *cv = (OsWin32CondVar *)c.v[0];
  WakeAllConditionVariable(&cv->cv);
}

typedef struct {
  SYNCHRONIZATION_BARRIER sb;
} OsWin32Barrier;

Barrier os_barrier_alloc(u32 count) {
  Barrier result = {0};
  if (count == 0)
    return result;
  OsWin32Barrier *barrier =
      (OsWin32Barrier *)os_allocate_memory(sizeof(OsWin32Barrier));
  if (!barrier)
    return result;
  if (!InitializeSynchronizationBarrier(&barrier->sb, count, -1)) {
    os_free_memory(barrier, sizeof(OsWin32Barrier));
    return result;
  }
  result.v[0] = (u64)barrier;
  return result;
}

void os_barrier_release(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsWin32Barrier *barrier = (OsWin32Barrier *)b.v[0];
  DeleteSynchronizationBarrier(&barrier->sb);
  os_free_memory(barrier, sizeof(OsWin32Barrier));
}

void os_barrier_wait(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsWin32Barrier *barrier = (OsWin32Barrier *)b.v[0];
  EnterSynchronizationBarrier(&barrier->sb, 0);
}

#define MAX_STACK_FRAMES 50
#define MAX_SYMBOL_LEN 512
#define CRASH_DUMP_DIR "crashes"

#pragma comment(lib, "dbghelp.lib")

static Mutex g_stack_trace_mutex = {0};
static b32 g_stack_trace_mutex_initialized = false;
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
  if (!g_stack_trace_mutex_initialized) {
    g_stack_trace_mutex = os_mutex_alloc();
    g_stack_trace_mutex_initialized = true;
  }

  os_mutex_take(g_stack_trace_mutex);

  ensure_symbols_initialized();

  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count =
      CaptureStackBackTrace(0, MAX_STACK_FRAMES, stack_frames, NULL);

  if (frame_count <= skip_frames) {
    os_mutex_drop(g_stack_trace_mutex);
    return;
  }

  char *stack_buffer = (char *)os_w32_state.stack_trace_buffer;

  write_stack_to_buffer(stack_buffer, KB(64), stack_frames, frame_count,
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

  os_mutex_drop(g_stack_trace_mutex);
}

static const char *get_exception_string(DWORD exception_code) {
  switch (exception_code) {
  case EXCEPTION_ACCESS_VIOLATION:
    return "ACCESS_VIOLATION";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    return "ARRAY_BOUNDS_EXCEEDED";
  case EXCEPTION_BREAKPOINT:
    return "BREAKPOINT";
  case EXCEPTION_DATATYPE_MISALIGNMENT:
    return "DATATYPE_MISALIGNMENT";
  case EXCEPTION_FLT_DENORMAL_OPERAND:
    return "FLT_DENORMAL_OPERAND";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    return "FLT_DIVIDE_BY_ZERO";
  case EXCEPTION_FLT_INEXACT_RESULT:
    return "FLT_INEXACT_RESULT";
  case EXCEPTION_FLT_INVALID_OPERATION:
    return "FLT_INVALID_OPERATION";
  case EXCEPTION_FLT_OVERFLOW:
    return "FLT_OVERFLOW";
  case EXCEPTION_FLT_STACK_CHECK:
    return "FLT_STACK_CHECK";
  case EXCEPTION_FLT_UNDERFLOW:
    return "FLT_UNDERFLOW";
  case EXCEPTION_ILLEGAL_INSTRUCTION:
    return "ILLEGAL_INSTRUCTION";
  case EXCEPTION_IN_PAGE_ERROR:
    return "IN_PAGE_ERROR";
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    return "INT_DIVIDE_BY_ZERO";
  case EXCEPTION_INT_OVERFLOW:
    return "INT_OVERFLOW";
  case EXCEPTION_INVALID_DISPOSITION:
    return "INVALID_DISPOSITION";
  case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    return "NONCONTINUABLE_EXCEPTION";
  case EXCEPTION_PRIV_INSTRUCTION:
    return "PRIV_INSTRUCTION";
  case EXCEPTION_SINGLE_STEP:
    return "SINGLE_STEP";
  case EXCEPTION_STACK_OVERFLOW:
    return "STACK_OVERFLOW";
  default:
    return "UNKNOWN_EXCEPTION";
  }
}

static LONG WINAPI
unhandled_exception_handler(EXCEPTION_POINTERS *exception_info) {
  if (!g_stack_trace_mutex_initialized) {
    g_stack_trace_mutex = os_mutex_alloc();
    g_stack_trace_mutex_initialized = true;
  }

  os_mutex_take(g_stack_trace_mutex);

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

  const char *exception_str =
      get_exception_string(exception_record->ExceptionCode);

  char header[1024];
  snprintf(header, sizeof(header),
           "\n===== FATAL EXCEPTION =====\n"
           "Exception: %s (0x%08lX)\n"
           "Address: 0x%p\n",
           exception_str, exception_record->ExceptionCode,
           exception_record->ExceptionAddress);

  if (exception_record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    const char *access_type =
        (exception_record->ExceptionInformation[0] == 0)   ? "reading"
        : (exception_record->ExceptionInformation[0] == 1) ? "writing"
                                                           : "executing";
    char access_info[256];
    snprintf(access_info, sizeof(access_info),
             "Access violation %s address: 0x%p\n", access_type,
             (void *)exception_record->ExceptionInformation[1]);
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
    if (!StackWalk64(machine_type, process, thread, &stack_frame, context, NULL,
                     SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
      break;
    }

    if (stack_frame.AddrPC.Offset == 0) {
      break;
    }

    stack_frames[frame_count++] = (void *)(uintptr_t)stack_frame.AddrPC.Offset;
  }

  char *stack_buffer = (char *)os_w32_state.stack_trace_buffer;
  write_stack_to_buffer(stack_buffer, KB(64), stack_frames, frame_count, 0);
  fprintf(console_output, "%s", stack_buffer);
  if (crash_file) {
    fprintf(crash_file, "%s", stack_buffer);
  }

  if (crash_file) {
    fprintf(crash_file, "\nRegisters:\n");
#ifdef _M_X64
    fprintf(crash_file, "RAX=%016llX RBX=%016llX RCX=%016llX\n", context->Rax,
            context->Rbx, context->Rcx);
    fprintf(crash_file, "RDX=%016llX RSI=%016llX RDI=%016llX\n", context->Rdx,
            context->Rsi, context->Rdi);
    fprintf(crash_file, "RIP=%016llX RSP=%016llX RBP=%016llX\n", context->Rip,
            context->Rsp, context->Rbp);
    fprintf(crash_file, "R8 =%016llX R9 =%016llX R10=%016llX\n", context->R8,
            context->R9, context->R10);
    fprintf(crash_file, "R11=%016llX R12=%016llX R13=%016llX\n", context->R11,
            context->R12, context->R13);
    fprintf(crash_file, "R14=%016llX R15=%016llX\n", context->R14,
            context->R15);
#else
    fprintf(crash_file, "EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", context->Eax,
            context->Ebx, context->Ecx, context->Edx);
    fprintf(crash_file, "ESI=%08X EDI=%08X EIP=%08X ESP=%08X\n", context->Esi,
            context->Edi, context->Eip, context->Esp);
    fprintf(crash_file, "EBP=%08X EFL=%08X\n", context->Ebp, context->EFlags);
#endif

    fclose(crash_file);
    fprintf(console_output, "\nCrash dump saved to: %s\n", crash_filename);
  }

  fprintf(console_output, "===========================\n");
  fflush(console_output);

  os_mutex_drop(g_stack_trace_mutex);

  if (g_previous_exception_filter) {
    return g_previous_exception_filter(exception_info);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

void os_install_crash_handler(void) {
  g_previous_exception_filter =
      SetUnhandledExceptionFilter(unhandled_exception_handler);

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
  HANDLE file = CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);

  if (file == INVALID_HANDLE_VALUE) {
    LOG_ERROR("Error opening file for writing: %", FMT_STR(file_path));
    return false;
  }

  LARGE_INTEGER size;
  size.QuadPart = (LONGLONG)buffer_len;
  SetFilePointerEx(file, size, NULL, FILE_BEGIN);
  SetEndOfFile(file);
  SetFilePointer(file, 0, NULL, FILE_BEGIN);

  DWORD written = 0;
  BOOL success = WriteFile(file, buffer, (DWORD)buffer_len, &written, NULL);
  if (!success || written != (DWORD)buffer_len) {
    LOG_ERROR("Error writing to file: %", FMT_STR(file_path));
    CloseHandle(file);
    return false;
  }

  CloseHandle(file);
  return true;
}

typedef NTSTATUS(NTAPI *PFN_NtCreateFile)(
    PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
    ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer,
    ULONG EaLength);
typedef NTSTATUS(NTAPI *PFN_NtClose)(HANDLE Handle);

static PFN_NtCreateFile g_NtCreateFile = NULL;
static PFN_NtClose g_NtClose = NULL;

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_OBJECT_NAME_COLLISION ((NTSTATUS)0xC0000035L)
#define STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC000003AL)

#ifndef FILE_DIRECTORY_FILE
#define FILE_DIRECTORY_FILE 0x00000001
#endif
#ifndef FILE_SYNCHRONOUS_IO_NONALERT
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#endif
#ifndef FILE_OPEN_IF
#define FILE_OPEN_IF 0x00000003
#endif

internal void os_init_nt_funcs(void) {
  if (!g_NtCreateFile) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    g_NtCreateFile = (PFN_NtCreateFile)GetProcAddress(ntdll, "NtCreateFile");
    g_NtClose = (PFN_NtClose)GetProcAddress(ntdll, "NtClose");
  }
}

internal NTSTATUS os_nt_create_single_dir(WCHAR *nt_path, u32 len) {
  UNICODE_STRING uni_path;
  uni_path.Buffer = nt_path;
  uni_path.Length = (USHORT)(len * sizeof(WCHAR));
  uni_path.MaximumLength = uni_path.Length + sizeof(WCHAR);

  OBJECT_ATTRIBUTES obj_attr;
  InitializeObjectAttributes(&obj_attr, &uni_path, OBJ_CASE_INSENSITIVE, NULL,
                             NULL);

  IO_STATUS_BLOCK io_status;
  HANDLE dir_handle;

  NTSTATUS status = g_NtCreateFile(
      &dir_handle, FILE_LIST_DIRECTORY | SYNCHRONIZE, &obj_attr, &io_status,
      NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
      FILE_OPEN_IF, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL,
      0);

  if (status == STATUS_SUCCESS) {
    g_NtClose(dir_handle);
  }
  return status;
}

bool32 os_create_dir(const char *dir_path) {
  os_init_nt_funcs();

  WCHAR wide_input[MAX_PATH];
  int input_len = MultiByteToWideChar(CP_UTF8, 0, dir_path, -1, wide_input, MAX_PATH);
  if (input_len == 0)
    return false;

  for (int i = 0; i < input_len; i++) {
    if (wide_input[i] == L'/')
      wide_input[i] = L'\\';
  }

  WCHAR abs_path[MAX_PATH];
  DWORD abs_len = GetFullPathNameW(wide_input, MAX_PATH, abs_path, NULL);
  if (abs_len == 0 || abs_len >= MAX_PATH)
    return false;

  WCHAR nt_path[MAX_PATH + 8];
  nt_path[0] = L'\\';
  nt_path[1] = L'?';
  nt_path[2] = L'?';
  nt_path[3] = L'\\';
  memcpy(nt_path + 4, abs_path, (abs_len + 1) * sizeof(WCHAR));

  u32 prefix_len = 4;
  u32 total_len = prefix_len + abs_len;

  while (total_len > prefix_len && nt_path[total_len - 1] == L'\\') {
    total_len--;
    nt_path[total_len] = L'\0';
  }

  NTSTATUS status = os_nt_create_single_dir(nt_path, total_len);

  if (status == STATUS_SUCCESS || status == STATUS_OBJECT_NAME_COLLISION) {
    return true;
  }

  if (status != STATUS_OBJECT_PATH_NOT_FOUND) {
    return false;
  }

  u32 sep_positions[64];
  u32 sep_count = 0;

  u32 start = prefix_len + 3;

  for (u32 i = start; i < total_len && sep_count < 64; i++) {
    if (nt_path[i] == L'\\') {
      sep_positions[sep_count++] = i;
    }
  }

  i32 first_missing = -1;
  for (i32 s = (i32)sep_count - 1; s >= 0; s--) {
    u32 pos = sep_positions[s];
    WCHAR saved = nt_path[pos];
    nt_path[pos] = L'\0';

    DWORD attrs = GetFileAttributesW(nt_path + 4);
    nt_path[pos] = saved;

    if (attrs != INVALID_FILE_ATTRIBUTES &&
        (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
      first_missing = s + 1;
      break;
    }
  }

  if (first_missing < 0) {
    first_missing = 0;
  }

  for (u32 s = (u32)first_missing; s < sep_count; s++) {
    u32 pos = sep_positions[s];
    WCHAR saved = nt_path[pos];
    nt_path[pos] = L'\0';

    status = os_nt_create_single_dir(nt_path, pos);
    nt_path[pos] = saved;

    if (status != STATUS_SUCCESS && status != STATUS_OBJECT_NAME_COLLISION) {
      return false;
    }
  }

  status = os_nt_create_single_dir(nt_path, total_len);
  return (status == STATUS_SUCCESS || status == STATUS_OBJECT_NAME_COLLISION);
}

PlatformFileData os_read_file(const char *file_path, Allocator *allocator) {
  PlatformFileData result = {0};

  HANDLE file =
      CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    LOG_ERROR("Failed to open file: %", FMT_STR(file_path));
    return result;
  }

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file, &file_size)) {
    LOG_ERROR("Failed to get file size: %", FMT_STR(file_path));
    CloseHandle(file);
    return result;
  }

  result.buffer = ALLOC_ARRAY(allocator, uint8, file_size.QuadPart);
  if (!result.buffer) {
    LOG_ERROR("Failed to allocate memory for file: %", FMT_STR(file_path));
    CloseHandle(file);
    return result;
  }

  DWORD bytes_read = 0;
  if (!ReadFile(file, result.buffer, (DWORD)file_size.QuadPart, &bytes_read,
                NULL) ||
      bytes_read != (DWORD)file_size.QuadPart) {
    LOG_ERROR("Failed to read file completely: %", FMT_STR(file_path));
    CloseHandle(file);
    return result;
  }

  CloseHandle(file);
  result.buffer_len = (uint32)file_size.QuadPart;
  result.success = true;
  return result;
}

struct OsFileOp {
  OsWin32Entity *entity;
};

internal void file_read_worker(void *data) {
  OsWin32Entity *entity = (OsWin32Entity *)data;

  HANDLE file =
      CreateFileA(entity->file_op.file_path, GENERIC_READ, FILE_SHARE_READ,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    ins_atomic_store_release(&entity->file_op.state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file, &file_size)) {
    CloseHandle(file);
    ins_atomic_store_release(&entity->file_op.state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  if (file_size.QuadPart < 0 || file_size.QuadPart > UINT32_MAX) {
    CloseHandle(file);
    ins_atomic_store_release(&entity->file_op.state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  // todo: solution to not allocate here
  u8 *buffer = os_allocate_memory((size_t)file_size.QuadPart);
  if (!buffer) {
    CloseHandle(file);
    ins_atomic_store_release(&entity->file_op.state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  DWORD bytes_read;
  BOOL read_success =
      ReadFile(file, buffer, (DWORD)file_size.QuadPart, &bytes_read, NULL);
  CloseHandle(file);

  if (read_success && bytes_read == (DWORD)file_size.QuadPart) {
    entity->file_op.buffer = buffer;
    entity->file_op.buffer_len = (u32)file_size.QuadPart;
    ins_atomic_store_release(&entity->file_op.state,
                             OS_FILE_READ_STATE_COMPLETED);
  } else {
    os_free_memory(buffer, file_size.QuadPart);
    ins_atomic_store_release(&entity->file_op.state, OS_FILE_READ_STATE_ERROR);
  }
}

OsFileOp *os_start_read_file(const char *file_path, TaskSystem *task_system) {
  if (!task_system)
    return NULL;

  OsWin32Entity *entity = os_w32_entity_alloc(OS_W32_ENTITY_FILE_OP);
  if (!entity)
    return NULL;

  size_t path_len = strlen(file_path);
  if (path_len >= MAX_PATH) {
    os_w32_entity_release(entity);
    return NULL;
  }

  memcpy(entity->file_op.file_path, file_path, path_len + 1);
  entity->file_op.state = OS_FILE_READ_STATE_IN_PROGRESS;
  entity->file_op.buffer = NULL;
  entity->file_op.buffer_len = 0;

  task_schedule(task_system, file_read_worker, entity);

  return (OsFileOp *)entity;
}

OsFileReadState os_check_read_file(OsFileOp *op) {
  if (!op)
    return OS_FILE_READ_STATE_ERROR;
  OsWin32Entity *entity = (OsWin32Entity *)op;
  return ins_atomic_load_acquire(&entity->file_op.state);
}

i32 os_get_file_size(OsFileOp *op) {
  if (!op)
    return -1;
  OsWin32Entity *entity = (OsWin32Entity *)op;
  OsFileReadState state = ins_atomic_load_acquire(&entity->file_op.state);
  return (state == OS_FILE_READ_STATE_COMPLETED)
             ? (i32)entity->file_op.buffer_len
             : -1;
}

b32 os_get_file_data(OsFileOp *op, _out_ PlatformFileData *data,
                     Allocator *allocator) {
  if (!op)
    return false;
  OsWin32Entity *entity = (OsWin32Entity *)op;
  OsFileReadState state = ins_atomic_load_acquire(&entity->file_op.state);

  if (state != OS_FILE_READ_STATE_COMPLETED || !entity->file_op.buffer) {
    return false;
  }

  data->buffer_len = entity->file_op.buffer_len;
  data->buffer = ALLOC_ARRAY(allocator, u8, entity->file_op.buffer_len);
  if (!data->buffer) {
    return false;
  }

  memcpy(data->buffer, entity->file_op.buffer, entity->file_op.buffer_len);
  data->success = true;

  os_free_memory(entity->file_op.buffer, entity->file_op.buffer_len);
  entity->file_op.buffer = NULL;
  entity->file_op.buffer_len = 0;

  os_w32_entity_release(entity);

  return true;
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
  WIN32_FILE_ATTRIBUTE_DATA attr_data;
  if (GetFileAttributesExA(path, GetFileExInfoStandard, &attr_data)) {
    ULARGE_INTEGER ull;
    ull.LowPart = attr_data.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = attr_data.ftLastWriteTime.dwHighDateTime;
    info.modification_time = (i64)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
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

  u32 src_len = str_len(src_path);
  u32 dst_len = str_len(dst_path);

  HANDLE dir_handle = CreateFileA(
      src_path, FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  b32 success = true;
  char buffer[32 * 1024];
  NT_IO_STATUS_BLOCK io;
  BOOLEAN first_call = TRUE;

  while (os_w32_state.NtQueryDirectoryFile &&
         os_w32_state.NtQueryDirectoryFile(dir_handle, NULL, NULL, NULL, &io,
                                           buffer, sizeof(buffer), 1, FALSE,
                                           NULL, first_call) == 0) {
    first_call = FALSE;
    NT_FILE_DIRECTORY_INFORMATION *entry = (NT_FILE_DIRECTORY_INFORMATION *)buffer;

    while (1) {
      u32 name_len_chars = entry->FileNameLength / sizeof(WCHAR);
      char name_utf8[MAX_PATH];
      int utf8_len = WideCharToMultiByte(CP_UTF8, 0, entry->FileName,
                                         name_len_chars, name_utf8,
                                         MAX_PATH - 1, NULL, NULL);
      name_utf8[utf8_len] = '\0';

      b32 is_dot = (utf8_len == 1 && name_utf8[0] == '.');
      b32 is_dotdot = (utf8_len == 2 && name_utf8[0] == '.' && name_utf8[1] == '.');

      if (!is_dot && !is_dotdot) {
        char src_full[MAX_PATH];
        char dst_full[MAX_PATH];
        memcpy(src_full, src_path, src_len);
        src_full[src_len] = '\\';
        memcpy(src_full + src_len + 1, name_utf8, utf8_len + 1);
        memcpy(dst_full, dst_path, dst_len);
        dst_full[dst_len] = '\\';
        memcpy(dst_full + dst_len + 1, name_utf8, utf8_len + 1);

        if (entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
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
      }

      if (!entry->NextEntryOffset) break;
      entry = (NT_FILE_DIRECTORY_INFORMATION *)((char *)entry + entry->NextEntryOffset);
    }
    if (!success) break;
  }

  CloseHandle(dir_handle);
  return success;
}

b32 os_directory_copy(const char *src_path, const char *dst_path) {
  return copy_directory_recursive(src_path, dst_path);
}

static b32 remove_directory_recursive(const char *path) {
  u32 path_len = str_len(path);

  HANDLE dir_handle = CreateFileA(
      path, FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  b32 success = true;
  char buffer[32 * 1024];
  NT_IO_STATUS_BLOCK io;
  BOOLEAN first_call = TRUE;

  while (os_w32_state.NtQueryDirectoryFile &&
         os_w32_state.NtQueryDirectoryFile(dir_handle, NULL, NULL, NULL, &io,
                                           buffer, sizeof(buffer), 1, FALSE,
                                           NULL, first_call) == 0) {
    first_call = FALSE;
    NT_FILE_DIRECTORY_INFORMATION *entry = (NT_FILE_DIRECTORY_INFORMATION *)buffer;

    while (1) {
      u32 name_len_chars = entry->FileNameLength / sizeof(WCHAR);
      char name_utf8[MAX_PATH];
      int utf8_len = WideCharToMultiByte(CP_UTF8, 0, entry->FileName,
                                         name_len_chars, name_utf8,
                                         MAX_PATH - 1, NULL, NULL);
      name_utf8[utf8_len] = '\0';

      b32 is_dot = (utf8_len == 1 && name_utf8[0] == '.');
      b32 is_dotdot = (utf8_len == 2 && name_utf8[0] == '.' && name_utf8[1] == '.');

      if (!is_dot && !is_dotdot) {
        char full_path[MAX_PATH];
        memcpy(full_path, path, path_len);
        full_path[path_len] = '\\';
        memcpy(full_path + path_len + 1, name_utf8, utf8_len + 1);

        if (entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
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
      }

      if (!entry->NextEntryOffset) break;
      entry = (NT_FILE_DIRECTORY_INFORMATION *)((char *)entry + entry->NextEntryOffset);
    }
    if (!success) break;
  }

  CloseHandle(dir_handle);

  if (success) {
    return RemoveDirectoryA(path) != 0;
  }
  return false;
}

b32 os_directory_remove(const char *path) {
  return remove_directory_recursive(path);
}

b32 os_system(const char *command) {
  STARTUPINFOA si = {0};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {0};

  char cmd_buf[4096];
  u32 len = str_len(command);
  if (len >= sizeof(cmd_buf)) return false;
  memcpy(cmd_buf, command, len + 1);

  if (!CreateProcessA(NULL, cmd_buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return exit_code == 0;
}

b32 os_symlink(const char *target_path, const char *link_path) {
  DeleteFileA(link_path);
  RemoveDirectoryA(link_path);

  char resolved[MAX_PATH];
  StringBuilder sb;
  sb_init(&sb, resolved, sizeof(resolved));

  u32 link_len = str_len(link_path);
  i32 last_slash = -1;
  for (i32 i = (i32)link_len - 1; i >= 0; i--) {
    if (link_path[i] == '/' || link_path[i] == '\\') {
      last_slash = i;
      break;
    }
  }

  if (last_slash >= 0 && target_path[0] == '.') {
    sb_append_len(&sb, link_path, (u32)last_slash + 1);
    sb_append(&sb, target_path);
  } else {
    sb_append(&sb, target_path);
  }

  DWORD attrs = GetFileAttributesA(sb_get(&sb));
  DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
  if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
    flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
  }

  char win_target[MAX_PATH];
  u32 target_len = str_len(target_path);
  for (u32 i = 0; i < target_len && i < MAX_PATH - 1; i++) {
    win_target[i] = (target_path[i] == '/') ? '\\' : target_path[i];
  }
  win_target[target_len < MAX_PATH ? target_len : MAX_PATH - 1] = '\0';

  return CreateSymbolicLinkA(link_path, win_target, flags) != 0;
}

b32 os_symlink_remove(const char *link_path) {
  DWORD attrs = GetFileAttributesA(link_path);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
      return RemoveDirectoryA(link_path) != 0;
    } else {
      return DeleteFileA(link_path) != 0;
    }
  }

  return false;
}

OsFileList os_list_files(const char *directory, const char *extension,
                         Allocator *allocator) {
  OsFileList result = {0};
  u32 dir_len = str_len(directory);
  u32 ext_len = str_len(extension);

  HANDLE dir_handle = CreateFileA(
      directory, FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir_handle == INVALID_HANDLE_VALUE) {
    return result;
  }

  int count = 0;
  int capacity = 256;

  u32 arena_size = (u32)(capacity * sizeof(char *) + capacity * MAX_PATH);
  char *arena = allocator->alloc_alloc(allocator->ctx, arena_size, 8);
  char **paths = (char **)arena;
  char *string_pool = arena + capacity * sizeof(char *);
  u32 pool_offset = 0;

  char buffer[64 * 1024];
  NT_IO_STATUS_BLOCK io;
  BOOLEAN first_call = TRUE;

  while (os_w32_state.NtQueryDirectoryFile && os_w32_state.NtQueryDirectoryFile(
             dir_handle, NULL, NULL, NULL, &io, buffer, sizeof(buffer),
             1, FALSE, NULL, first_call) == 0) {
    first_call = FALSE;
    NT_FILE_DIRECTORY_INFORMATION *entry = (NT_FILE_DIRECTORY_INFORMATION *)buffer;

    while (1) {
      if (!(entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        u32 name_len_chars = entry->FileNameLength / sizeof(WCHAR);
        char name_utf8[MAX_PATH];
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, entry->FileName,
                                           name_len_chars, name_utf8,
                                           MAX_PATH - 1, NULL, NULL);
        name_utf8[utf8_len] = '\0';

        b32 matches_ext = (ext_len == 0);
        if (!matches_ext && utf8_len >= (int)ext_len) {
          matches_ext = true;
          for (u32 i = 0; i < ext_len; i++) {
            if (name_utf8[utf8_len - ext_len + i] != extension[i]) {
              matches_ext = false;
              break;
            }
          }
        }

        if (matches_ext && count < capacity) {
          char *full_path = string_pool + pool_offset;
          memcpy(full_path, directory, dir_len);
          full_path[dir_len] = '/';
          memcpy(full_path + dir_len + 1, name_utf8, utf8_len + 1);
          paths[count++] = full_path;
          pool_offset += dir_len + utf8_len + 2;
        }
      }

      if (!entry->NextEntryOffset) break;
      entry = (NT_FILE_DIRECTORY_INFORMATION *)((char *)entry + entry->NextEntryOffset);
    }
  }

  CloseHandle(dir_handle);

  result.paths = paths;
  result.count = count;
  return result;
}

OsFileList os_list_dirs(const char *directory, Allocator *allocator) {
  OsFileList result = {0};
  u32 dir_len = str_len(directory);

  HANDLE dir_handle = CreateFileA(
      directory, FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir_handle == INVALID_HANDLE_VALUE) {
    return result;
  }

  int count = 0;
  int capacity = 256;

  u32 arena_size = (u32)(capacity * sizeof(char *) + capacity * MAX_PATH);
  char *arena = allocator->alloc_alloc(allocator->ctx, arena_size, 8);
  char **paths = (char **)arena;
  char *string_pool = arena + capacity * sizeof(char *);
  u32 pool_offset = 0;

  char buffer[64 * 1024];
  NT_IO_STATUS_BLOCK io;
  BOOLEAN first_call = TRUE;

  while (os_w32_state.NtQueryDirectoryFile && os_w32_state.NtQueryDirectoryFile(
             dir_handle, NULL, NULL, NULL, &io, buffer, sizeof(buffer),
             1, FALSE, NULL, first_call) == 0) {
    first_call = FALSE;
    NT_FILE_DIRECTORY_INFORMATION *entry = (NT_FILE_DIRECTORY_INFORMATION *)buffer;

    while (1) {
      if (entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        u32 name_len_chars = entry->FileNameLength / sizeof(WCHAR);
        char name_utf8[MAX_PATH];
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, entry->FileName,
                                           name_len_chars, name_utf8,
                                           MAX_PATH - 1, NULL, NULL);
        name_utf8[utf8_len] = '\0';

        b32 is_dot = (utf8_len == 1 && name_utf8[0] == '.');
        b32 is_dotdot = (utf8_len == 2 && name_utf8[0] == '.' && name_utf8[1] == '.');

        if (!is_dot && !is_dotdot && count < capacity) {
          char *full_path = string_pool + pool_offset;
          memcpy(full_path, directory, dir_len);
          full_path[dir_len] = '/';
          memcpy(full_path + dir_len + 1, name_utf8, utf8_len + 1);
          paths[count++] = full_path;
          pool_offset += dir_len + utf8_len + 2;
        }
      }

      if (!entry->NextEntryOffset) break;
      entry = (NT_FILE_DIRECTORY_INFORMATION *)((char *)entry + entry->NextEntryOffset);
    }
  }

  CloseHandle(dir_handle);

  result.paths = paths;
  result.count = count;
  return result;
}

b32 os_file_set_executable(const char *path) {
  UNUSED(path);
  return true;
}

char *os_cwd(char *buffer, u32 buffer_size) {
  DWORD len = GetCurrentDirectoryA(buffer_size, buffer);
  if (len == 0 || len >= buffer_size) {
    return NULL;
  }
  for (u32 i = 0; i < len; i++) {
    if (buffer[i] == '\\') {
      buffer[i] = '/';
    }
  }
  return buffer;
}

void os_time_init(void) { os_init(); }

u64 os_time_now(void) {
  os_w32_assert_state_initialized();
  LARGE_INTEGER qpc_t;
  QueryPerformanceCounter(&qpc_t);
  i64 elapsed = qpc_t.QuadPart - os_w32_state.time_start.QuadPart;
  return (u64)((f64)elapsed * os_w32_state.time_freq_inv_ns);
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

void os_sleep(u64 microseconds) {
  if (os_w32_state.NtDelayExecution) {
    LARGE_INTEGER delay;
    delay.QuadPart = -((i64)microseconds * 10);
    os_w32_state.NtDelayExecution(FALSE, &delay);
  } else {
    Sleep((DWORD)(microseconds / 1000));
  }
}

i32 os_get_processor_count(void) {
  os_w32_assert_state_initialized();
  return (i32)os_w32_state.processor_count;
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

u8 *os_reserve_memory(size_t size) {
  void *memory = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
  if (!memory) {
    DWORD err = GetLastError();
    LOG_ERROR("VirtualAlloc (reserve) failed. Size: %, Error: %",
              FMT_UINT(size), FMT_UINT(err));
    return NULL;
  }
  return memory;
}

b32 os_commit_memory(void *ptr, size_t size) {
  void *result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
  if (!result) {
    DWORD err = GetLastError();
    LOG_ERROR("VirtualAlloc (commit) failed. Size: %, Error: %", FMT_UINT(size),
              FMT_UINT(err));
    return false;
  }
  return true;
}

u32 os_get_page_size(void) {
  os_w32_assert_state_initialized();
  return os_w32_state.page_size;
}

const char *os_get_compressed_texture_format_suffix(void) { return "_dxt5"; }

OsKeyboardRect os_get_keyboard_rect(f32 time) {
  UNUSED(time);
  OsKeyboardRect rect = {0};
  return rect;
}

OsSafeAreaInsets os_get_safe_area(void) {
  OsSafeAreaInsets insets = {0};
  return insets;
}

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
  return 0;
}

int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                               int32 buffer_len, _out_ bool32 *is_final) {
  UNUSED(op_id);
  UNUSED(buffer);
  UNUSED(buffer_len);
  UNUSED(is_final);
  return -1;
}

u32 os_mic_get_available_samples(void) { return 0; }
u32 os_mic_read_samples(i16 *buffer, u32 max_samples) {
  UNUSED(buffer);
  UNUSED(max_samples);
  return 0;
}
void os_mic_start_recording(void) {}
void os_mic_stop_recording(void) {}
u32 os_mic_get_sample_rate(void) { return 48000; }
#include "os_win32_video.c"
