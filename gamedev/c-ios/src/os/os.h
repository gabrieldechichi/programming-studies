#pragma once

#include "../src/typedefs.h"

void os_init(void);

u64 os_time_now(void);

u64 os_time_diff(u64 new_ticks, u64 old_ticks);

f64 os_ticks_to_ms(u64 ticks);

f64 os_ticks_to_us(u64 ticks);

f64 os_ticks_to_ns(u64 ticks);

void os_sleep_us(u32 microseconds);
