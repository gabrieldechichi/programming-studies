#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <os/log.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <mach-o/arch.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

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
} CrashReport;

static os_log_t hz_log;
static int g_crash_fd = -1;

const char* ios_get_documents_path(const char *relative_path);

__attribute__((constructor))
static void init_logging(void) {
  hz_log = os_log_create("hz-engine", "main");
}

static void ios_crash_signal_handler(int sig, siginfo_t *info, void *context) {
  (void)context;

  CrashReport report;
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

void ios_install_crash_handlers(void) {
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

void ios_check_pending_crash_report(void) {
  const char* crash_path = ios_get_documents_path("pending_crash.bin");

  NSLog(@"Checking for pending crash at: %s", crash_path);

  int fd = open(crash_path, O_RDONLY);
  if (fd < 0) {
    NSLog(@"No pending crash file found (errno: %d)", errno);
    return;
  }

  NSLog(@"Found pending crash file, reading...");

  CrashReport report;
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
  
  // Check if it's already an absolute path
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

void ios_log_message(const char *message) {
  os_log(hz_log, "%{public}s", message);
  // fprintf(stderr, "%s\n", message);
  // fflush(stderr);
}

int ios_get_thermal_state(void) {
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

void ios_disable_idle_timer(void) {
  [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
  NSLog(@"iOS idle timer disabled - screen will stay awake");
}