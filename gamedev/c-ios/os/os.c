#ifdef LINUX
#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
#endif

#include "os.h"
#include <stdbool.h>

#ifdef MACOS
#include <mach/mach_time.h>
#include <unistd.h>
#elif defined(LINUX)
#include <time.h>
#include <unistd.h>
#endif

#ifdef MACOS
static mach_timebase_info_data_t timebase = {0};
static bool timebase_initialized = false;

static void ensure_timebase_initialized(void) {
    if (!timebase_initialized) {
        mach_timebase_info(&timebase);
        timebase_initialized = true;
    }
}
#endif

void os_init(void) {
#ifdef MACOS
    ensure_timebase_initialized();
#endif
}

u64 os_time_now(void) {
#ifdef MACOS
    return mach_absolute_time();
#elif defined(LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
#endif
}

u64 os_time_diff(u64 new_ticks, u64 old_ticks) {
    return new_ticks - old_ticks;
}

f64 os_ticks_to_ms(u64 ticks) {
#ifdef MACOS
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom) / 1000000.0;
#elif defined(LINUX)
    return (f64)ticks / 1000000.0;  // Convert nanoseconds to milliseconds
#else
    (void)ticks;
    return 0.0;
#endif
}

f64 os_ticks_to_us(u64 ticks) {
#ifdef MACOS
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom) / 1000.0;
#elif defined(LINUX)
    return (f64)ticks / 1000.0;  // Convert nanoseconds to microseconds
#else
    (void)ticks;
    return 0.0;
#endif
}

f64 os_ticks_to_ns(u64 ticks) {
#ifdef MACOS
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom);
#elif defined(LINUX)
    return (f64)ticks;  // Already in nanoseconds
#else
    (void)ticks;
    return 0.0;
#endif
}

void os_sleep_us(u32 microseconds) {
#ifdef MACOS
    usleep(microseconds);
#elif defined(LINUX)
    usleep(microseconds);
#endif
}
