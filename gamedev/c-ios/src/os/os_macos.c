#include "os.h"
#include <stdbool.h>
#include <mach/mach_time.h>
#include <unistd.h>

static mach_timebase_info_data_t timebase = {0};
static bool timebase_initialized = false;

static void ensure_timebase_initialized(void) {
    if (!timebase_initialized) {
        mach_timebase_info(&timebase);
        timebase_initialized = true;
    }
}

void platform_init(void) {
    ensure_timebase_initialized();
}

u64 platform_time_now(void) {
    return mach_absolute_time();
}

u64 platform_time_diff(u64 new_ticks, u64 old_ticks) {
    return new_ticks - old_ticks;
}

f64 platform_ticks_to_ms(u64 ticks) {
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom) / 1000000.0;
}

f64 platform_ticks_to_us(u64 ticks) {
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom) / 1000.0;
}

f64 platform_ticks_to_ns(u64 ticks) {
    ensure_timebase_initialized();
    return (f64)(ticks * timebase.numer / timebase.denom);
}

void platform_sleep_us(u32 microseconds) {
    usleep(microseconds);
}