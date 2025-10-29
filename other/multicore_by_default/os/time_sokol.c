#include "lib/typedefs.h"
#if !defined(WIN64) && !defined(MACOS)
#include "sokol/sokol_time.h"
#endif

/*!
 * Platform timing
 * */
#ifdef BUILD_SYSTEM
void os_time_init(void) {
}

u64 os_time_now(void) {
    return 0;
}

u64 os_time_diff(u64 new_ticks, u64 old_ticks) {
    (void)new_ticks;
    (void)old_ticks;
    return 0;
}

f64 os_ticks_to_ms(u64 ticks) {
    (void)ticks;
    return 0.0;
}

f64 os_ticks_to_us(u64 ticks) {
    (void)ticks;
    return 0.0;
}

f64 os_ticks_to_ns(u64 ticks) {
    (void)ticks;
    return 0.0;
}
#elif defined(WIN64)

#elif defined(MACOS)

#else

void os_time_init(void) {
    stm_setup();
}

u64 os_time_now(void) {
    return stm_now();
}

u64 os_time_diff(u64 new_ticks, u64 old_ticks) {
    return stm_diff(new_ticks, old_ticks);
}

f64 os_ticks_to_ms(u64 ticks) {
    return stm_ms(ticks);
}

f64 os_ticks_to_us(u64 ticks) {
    return stm_us(ticks);
}

f64 os_ticks_to_ns(u64 ticks) {
    return stm_ns(ticks);
}
#endif
