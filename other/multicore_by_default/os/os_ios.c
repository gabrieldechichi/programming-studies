#include "../lib/common.h"
#include "os.h"
#include "lib/string.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "sokol/sokol_app.h"

#include "os_darwin_common.c"
#include "os_darwin_http.c"
#include "microphone_darwin.c"
#include "os_video_darwin.c"

#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <os/log.h>

#pragma pop_macro("global")
#pragma pop_macro("internal")

#undef os_log

#include <signal.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <mach-o/arch.h>
#include <fcntl.h>
#include <time.h>

typedef struct {
    char magic[8];
    uint32_t version;
    int signal_number;
    void* fault_address;
    uint64_t timestamp;
    char binary_name[64];
    intptr_t aslr_slide;
    char arch_name[16];
    int frame_count;
    void* addresses[128];
} CrashReportData;

static os_log_t hz_log;
static int g_crash_fd = -1;

const char* ios_get_documents_path(const char *relative_path);

__attribute__((constructor))
static void init_logging(void) {
  hz_log = os_log_create("hz-engine", "main");
}

internal void ios_crash_signal_handler(int sig, siginfo_t *info, void *context) {
  (void)context;

  CrashReportData report;
  memset(&report, 0, sizeof(report));

  strncpy(report.magic, "HZCRASH", sizeof(report.magic));
  report.version = 1;
  report.signal_number = sig;
  report.fault_address = info->si_addr;
  report.timestamp = (uint64_t)time(NULL);

  report.frame_count = backtrace(report.addresses, 128);

  const char *exec_name = NULL;
  uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; i++) {
    const struct mach_header *header = _dyld_get_image_header(i);
    if (header && header->filetype == MH_EXECUTE) {
      exec_name = _dyld_get_image_name(i);
      report.aslr_slide = _dyld_get_image_vmaddr_slide(i);
      break;
    }
  }

  const char *binary_name = "hz-engine";
  if (exec_name) {
    const char *last_slash = strrchr(exec_name, '/');
    if (last_slash) {
      binary_name = last_slash + 1;
    }
  }
  strncpy(report.binary_name, binary_name, sizeof(report.binary_name) - 1);

  const NXArchInfo *arch_info = NXGetLocalArchInfo();
  const char *arch_name = arch_info ? arch_info->name : "arm64";
  strncpy(report.arch_name, arch_name, sizeof(report.arch_name) - 1);

  if (g_crash_fd >= 0) {
    ssize_t written = write(g_crash_fd, &report, sizeof(report));
    fsync(g_crash_fd);

    char msg[128];
    snprintf(msg, sizeof(msg), "Wrote %zd bytes to crash file (fd=%d)\n", written, g_crash_fd);
    write(2, msg, strlen(msg));
  } else {
    const char* err_msg = "ERROR: crash_fd not open!\n";
    write(2, err_msg, strlen(err_msg));
  }

#ifdef DEBUG
  static char buffer[8192];
  int pos = 0;

  const char *signal_name = "UNKNOWN";
  switch (sig) {
    case SIGTRAP: signal_name = "SIGTRAP"; break;
    case SIGSEGV: signal_name = "SIGSEGV"; break;
    case SIGABRT: signal_name = "SIGABRT"; break;
    case SIGBUS: signal_name = "SIGBUS"; break;
    case SIGILL: signal_name = "SIGILL"; break;
    case SIGFPE: signal_name = "SIGFPE"; break;
  }

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "\n========== CRASH: %s (signal %d) ==========\n",
                  signal_name, sig);

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "Fault address: %p\n", info->si_addr);

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "Binary: %s (slide: 0x%lx)\n", binary_name, report.aslr_slide);

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "\n=== ATOS COMMAND (copy entire line below) ===\n");

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "atos -arch %s -o YOUR_DSYM_PATH_HERE -l 0x%lx",
                  arch_name, 0x100000000L + report.aslr_slide);

  for (int i = 0; i < report.frame_count && pos < (int)(sizeof(buffer) - 20); i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                    " 0x%lx", (unsigned long)report.addresses[i]);
  }

  pos += snprintf(buffer + pos, sizeof(buffer) - pos, "\n");

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "==============================================\n");

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "\nRAW ADDRESSES:\n");
  for (int i = 0; i < report.frame_count && i < 20 && pos < (int)(sizeof(buffer) - 50); i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                    "  [%2d]: 0x%lx\n", i, (unsigned long)report.addresses[i]);
  }

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "\n==============================================\n");

  os_log_fault(hz_log, "%{public}s", buffer);
#endif

  signal(sig, SIG_DFL);
  raise(sig);
}

internal void ios_install_crash_handlers(void) {
  NSLog(@"Installing iOS crash handlers...");

  const char* crash_path = ios_get_documents_path("pending_crash.bin");
  NSLog(@"Opening crash file at: %s", crash_path);
  g_crash_fd = open(crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  if (g_crash_fd < 0) {
    NSLog(@"Warning: Failed to open crash report file (errno: %d)", errno);
  } else {
    NSLog(@"Crash file opened successfully (fd=%d)", g_crash_fd);
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = ios_crash_signal_handler;

  sigaction(SIGTRAP, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);

  NSLog(@"Crash handlers installed successfully");
}

internal void ios_check_pending_crash_report(void) {
  const char* crash_path = ios_get_documents_path("pending_crash.bin");

  NSLog(@"Checking for pending crash at: %s", crash_path);

  int fd = open(crash_path, O_RDONLY);
  if (fd < 0) {
    NSLog(@"No pending crash file found (errno: %d)", errno);
    return;
  }

  NSLog(@"Found pending crash file, reading...");

  CrashReportData report;
  ssize_t bytes_read = read(fd, &report, sizeof(report));
  close(fd);

  if (bytes_read != sizeof(report)) {
    unlink(crash_path);
    return;
  }

  if (strncmp(report.magic, "HZCRASH", 7) != 0) {
    unlink(crash_path);
    return;
  }

  const char *signal_name = "UNKNOWN";
  switch (report.signal_number) {
    case SIGTRAP: signal_name = "SIGTRAP"; break;
    case SIGSEGV: signal_name = "SIGSEGV"; break;
    case SIGABRT: signal_name = "SIGABRT"; break;
    case SIGBUS: signal_name = "SIGBUS"; break;
    case SIGILL: signal_name = "SIGILL"; break;
    case SIGFPE: signal_name = "SIGFPE"; break;
  }

  NSMutableString *output = [NSMutableString string];
  [output appendString:@"\n========================================\n"];
  [output appendFormat:@"PREVIOUS CRASH DETECTED: %s (signal %d)\n", signal_name, report.signal_number];
  [output appendFormat:@"Timestamp: %llu\n", report.timestamp];
  [output appendFormat:@"Fault address: %p\n", report.fault_address];
  [output appendFormat:@"Binary: %s (slide: 0x%lx)\n", report.binary_name, report.aslr_slide];
  [output appendString:@"\n=== ATOS COMMAND (copy entire line below) ===\n"];
  [output appendFormat:@"atos -arch %s -o YOUR_DSYM_PATH_HERE -l 0x%lx",
           report.arch_name, 0x100000000L + report.aslr_slide];

  for (int i = 0; i < report.frame_count; i++) {
    [output appendFormat:@" 0x%lx", (unsigned long)report.addresses[i]];
  }

  [output appendString:@"\n==============================================\n"];

  [output appendString:@"\nRAW ADDRESSES:\n"];
  int limit = report.frame_count < 20 ? report.frame_count : 20;
  for (int i = 0; i < limit; i++) {
    [output appendFormat:@"  [%2d]: 0x%lx\n", i, (unsigned long)report.addresses[i]];
  }

  [output appendString:@"\n==============================================\n"];

  os_log_fault(hz_log, "%{public}s", [output UTF8String]);
  NSLog(@"%@", output);

  unlink(crash_path);
}

const char* ios_get_bundle_resource_path(const char *relative_path) {
  static char path_buffer[1024];

  if (relative_path[0] == '/') {
    return relative_path;
  }

  NSString *path = [NSString stringWithUTF8String:relative_path];
  NSString *bundlePath = [[NSBundle mainBundle] resourcePath];
  NSString *fullPath = [bundlePath stringByAppendingPathComponent:path];

  strncpy(path_buffer, [fullPath UTF8String], sizeof(path_buffer) - 1);
  path_buffer[sizeof(path_buffer) - 1] = '\0';

  return path_buffer;
}

const char* ios_get_documents_path(const char *relative_path) {
  static char path_buffer[1024];

  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirectory = [paths objectAtIndex:0];
  NSString *fileName = [NSString stringWithUTF8String:relative_path];
  NSString *fullPath = [documentsDirectory stringByAppendingPathComponent:fileName];

  strncpy(path_buffer, [fullPath UTF8String], sizeof(path_buffer) - 1);
  path_buffer[sizeof(path_buffer) - 1] = '\0';

  return path_buffer;
}

internal void ios_log_message(const char *message) {
  os_log_with_type(hz_log, OS_LOG_TYPE_DEFAULT, "%{public}s", message);
}

internal int ios_get_thermal_state(void) {
  NSProcessInfoThermalState thermalState = [[NSProcessInfo processInfo] thermalState];

  switch (thermalState) {
    case NSProcessInfoThermalStateNominal:
      return 1;
    case NSProcessInfoThermalStateFair:
      return 2;
    case NSProcessInfoThermalStateSerious:
      return 3;
    case NSProcessInfoThermalStateCritical:
      return 4;
    default:
      return 0;
  }
}

internal void ios_disable_idle_timer(void) {
  [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
  NSLog(@"iOS idle timer disabled - screen will stay awake");
}

static CGRect g_keyboard_frame_start = {{0, 0}, {0, 0}};
static CGRect g_keyboard_frame_end = {{0, 0}, {0, 0}};
static CGRect g_keyboard_frame_current = {{0, 0}, {0, 0}};
static float g_keyboard_anim_start_time = 0.0f;
static float g_keyboard_anim_duration = 0.0f;
static bool g_keyboard_has_animation = false;
static float g_keyboard_curve_x1 = 0.0f;
static float g_keyboard_curve_y1 = 0.0f;
static float g_keyboard_curve_x2 = 0.0f;
static float g_keyboard_curve_y2 = 0.0f;

internal float ios_cubic_bezier(float t, float p1, float p2) {
  float u = 1.0f - t;
  float tt = t * t;
  float uu = u * u;
  float ttt = tt * t;
  return 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt;
}

internal void ios_set_curve_for_type(NSInteger curve) {
  CAMediaTimingFunction *timingFunc = nil;
  switch (curve) {
    case UIViewAnimationCurveEaseInOut:
      timingFunc = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
      break;
    case UIViewAnimationCurveEaseIn:
      timingFunc = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
      break;
    case UIViewAnimationCurveEaseOut:
      timingFunc = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
      break;
    case UIViewAnimationCurveLinear:
      timingFunc = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
      break;
    case 7:
      timingFunc = [CAMediaTimingFunction functionWithControlPoints:0.25f :0.1f :0.25f :1.0f];
      break;
    default:
      timingFunc = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionDefault];
      break;
  }

  float cp1[2], cp2[2];
  [timingFunc getControlPointAtIndex:1 values:cp1];
  [timingFunc getControlPointAtIndex:2 values:cp2];
  g_keyboard_curve_x1 = cp1[0];
  g_keyboard_curve_y1 = cp1[1];
  g_keyboard_curve_x2 = cp2[0];
  g_keyboard_curve_y2 = cp2[1];
}

internal float ios_keyboard_curve(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 10; i++) {
    float mid = (lo + hi) * 0.5f;
    float x = ios_cubic_bezier(mid, g_keyboard_curve_x1, g_keyboard_curve_x2);
    if (x < t) lo = mid;
    else hi = mid;
  }
  float param = (lo + hi) * 0.5f;
  return ios_cubic_bezier(param, g_keyboard_curve_y1, g_keyboard_curve_y2);
}

internal void ios_keyboard_notification_handler(NSNotification *notif, float current_time) {
  NSDictionary* info = notif.userInfo;
  CGRect frameEnd = [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
  NSTimeInterval duration = [[info objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  NSInteger curveValue = [[info objectForKey:UIKeyboardAnimationCurveUserInfoKey] integerValue];

  g_keyboard_frame_start = g_keyboard_frame_current;
  g_keyboard_frame_end = frameEnd;
  g_keyboard_anim_start_time = current_time;
  g_keyboard_anim_duration = (float)duration;
  g_keyboard_has_animation = true;
  ios_set_curve_for_type(curveValue);
}

static float g_pending_keyboard_time = 0.0f;

internal void ios_set_keyboard_time(float time) {
  g_pending_keyboard_time = time;
}

internal void ios_init_keyboard_tracking(void) {
  [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillShowNotification
    object:nil
    queue:[NSOperationQueue mainQueue]
    usingBlock:^(NSNotification *notif) {
      ios_keyboard_notification_handler(notif, g_pending_keyboard_time);
    }];

  [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillHideNotification
    object:nil
    queue:[NSOperationQueue mainQueue]
    usingBlock:^(NSNotification *notif) {
      NSDictionary* info = notif.userInfo;
      NSTimeInterval duration = [[info objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue];
      NSInteger curveValue = [[info objectForKey:UIKeyboardAnimationCurveUserInfoKey] integerValue];
      g_keyboard_frame_start = g_keyboard_frame_current;
      g_keyboard_frame_end = CGRectMake(0, 0, 0, 0);
      g_keyboard_anim_start_time = g_pending_keyboard_time;
      g_keyboard_anim_duration = (float)duration;
      g_keyboard_has_animation = true;
      ios_set_curve_for_type(curveValue);
    }];

  [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillChangeFrameNotification
    object:nil
    queue:[NSOperationQueue mainQueue]
    usingBlock:^(NSNotification *notif) {
      ios_keyboard_notification_handler(notif, g_pending_keyboard_time);
    }];
}

internal void ios_get_keyboard_frame(float current_time, float *x, float *y, float *width, float *height) {
  if (!g_keyboard_has_animation) {
    *x = 0;
    *y = 0;
    *width = 0;
    *height = 0;
    return;
  }

  float progress = 1.0f;
  if (g_keyboard_anim_duration > 0.0f) {
    float elapsed = current_time - g_keyboard_anim_start_time;
    progress = elapsed / g_keyboard_anim_duration;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    progress = ios_keyboard_curve(progress);
  }

  float inv = 1.0f - progress;
  *x = g_keyboard_frame_start.origin.x * inv + g_keyboard_frame_end.origin.x * progress;
  *y = g_keyboard_frame_start.origin.y * inv + g_keyboard_frame_end.origin.y * progress;
  *width = g_keyboard_frame_start.size.width * inv + g_keyboard_frame_end.size.width * progress;
  *height = g_keyboard_frame_start.size.height * inv + g_keyboard_frame_end.size.height * progress;

  g_keyboard_frame_current = CGRectMake(*x, *y, *width, *height);
}

internal void ios_get_safe_area_insets(float *top, float *left, float *bottom, float *right) {
  UIWindow *window = [[UIApplication sharedApplication] keyWindow];
  if (!window) {
    NSArray *windows = [[UIApplication sharedApplication] windows];
    if (windows.count > 0) {
      window = windows[0];
    }
  }

  if (window) {
    UIEdgeInsets insets = window.safeAreaInsets;
    *top = (float)insets.top;
    *left = (float)insets.left;
    *bottom = (float)insets.bottom;
    *right = (float)insets.right;
  } else {
    *top = 0.0f;
    *left = 0.0f;
    *bottom = 0.0f;
    *right = 0.0f;
  }
}

b32 os_is_mobile() { return true; }

OSThermalState os_get_thermal_state(void) {
  return (OSThermalState)ios_get_thermal_state();
}

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
                  const char *file_name, uint32 line_number) {
  char buffer[KB(128)];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "HZ_INFO";
    break;
  case LOGLEVEL_WARN:
    level_str = "HZ_WARN";
    break;
  case LOGLEVEL_ERROR:
    level_str = "HZ_ERROR";
    break;
  default:
    level_str = "HZ_UNKNOWN";
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

const char *os_get_compressed_texture_format_suffix(void) {
  return "_astc";
}

void os_show_keyboard(b32 show, f32 time) {
  static b32 initialized = false;
  if (!initialized) {
    ios_init_keyboard_tracking();
    initialized = true;
  }
  ios_set_keyboard_time(time);
  sapp_show_keyboard(show);
}

OsKeyboardRect os_get_keyboard_rect(f32 time) {
  OsKeyboardRect rect;
  ios_get_keyboard_frame(time, &rect.x, &rect.y, &rect.width, &rect.height);
  return rect;
}

OsSafeAreaInsets os_get_safe_area(void) {
  OsSafeAreaInsets insets;
  ios_get_safe_area_insets(&insets.top, &insets.left, &insets.bottom, &insets.right);
  return insets;
}
