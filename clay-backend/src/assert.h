#ifndef ASSERT_H
#define ASSERT_H

#if defined(__wasm__) && defined(__clang__)

// Assert - checks condition and traps if false (compiles to WASM 'unreachable' instruction)
#ifndef assert
#define assert(expr) ((expr) ? (void)0 : __builtin_trap())
#endif

// Static assert - compile-time assertion
#define static_assert(expr, msg) _Static_assert(expr, msg)

// Unreachable - marks code path as unreachable (optimization + runtime trap)
#define unreachable() __builtin_unreachable()

#ifdef DEBUG
#define debug_assert(expr) assert(expr)
#else
#define debug_assert(expr)
#endif

#else
#include <assert.h>
#endif

#endif // ASSERT_H
