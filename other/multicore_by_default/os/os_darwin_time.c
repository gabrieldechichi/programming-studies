#undef internal

#include <mach/mach_time.h>

static struct {
  mach_timebase_info_data_t timebase;
  u64 start;
  b32 initialized;
} g_time_state = {0};

static int64_t _stm_int64_muldiv(int64_t value, int64_t numer, int64_t denom) {
  int64_t q = value / denom;
  int64_t r = value % denom;
  return q * numer + r * numer / denom;
}

void os_time_init(void) {
  mach_timebase_info(&g_time_state.timebase);
  g_time_state.start = mach_absolute_time();
  g_time_state.initialized = true;
}

u64 os_time_now(void) {
  assert(g_time_state.initialized);
  const u64 mach_now = mach_absolute_time() - g_time_state.start;
  u64 now = (u64)_stm_int64_muldiv((int64_t)mach_now, (int64_t)g_time_state.timebase.numer, (int64_t)g_time_state.timebase.denom);
  return now;
}

u64 os_time_diff(u64 new_ticks, u64 old_ticks) {
  if (new_ticks > old_ticks) {
    return new_ticks - old_ticks;
  } else {
    return 1;
  }
}

f64 os_ticks_to_ms(u64 ticks) {
  return (f64)ticks / 1000000.0;
}

f64 os_ticks_to_us(u64 ticks) {
  return (f64)ticks / 1000.0;
}

f64 os_ticks_to_ns(u64 ticks) {
  return (f64)ticks;
}

#define internal static
