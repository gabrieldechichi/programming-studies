#pragma once

#include "lib/typedefs.h"

void platform_init(void);

u64 platform_time_now(void);
u64 platform_time_diff(u64 new_ticks, u64 old_ticks);
f64 platform_ticks_to_ms(u64 ticks);
f64 platform_ticks_to_us(u64 ticks);
f64 platform_ticks_to_ns(u64 ticks);

void platform_sleep_us(u32 microseconds);
