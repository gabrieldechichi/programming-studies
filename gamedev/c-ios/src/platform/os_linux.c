#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include "os.h"
#include <time.h>
#include <unistd.h>

void platform_init(void) {
    // No initialization needed for Linux
}

u64 platform_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

u64 platform_time_diff(u64 new_ticks, u64 old_ticks) {
    return new_ticks - old_ticks;
}

f64 platform_ticks_to_ms(u64 ticks) {
    return (f64)ticks / 1000000.0;  // Convert nanoseconds to milliseconds
}

f64 platform_ticks_to_us(u64 ticks) {
    return (f64)ticks / 1000.0;  // Convert nanoseconds to microseconds
}

f64 platform_ticks_to_ns(u64 ticks) {
    return (f64)ticks;  // Already in nanoseconds
}

void platform_sleep_us(u32 microseconds) {
    usleep(microseconds);
}