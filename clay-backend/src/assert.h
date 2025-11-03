#ifndef ASSERT_H
#define ASSERT_H

#if defined(__wasm__) && defined(__clang__)

// Assert - checks condition and traps if false (compiles to WASM 'unreachable' instruction)
#define assert(expr) ((expr) ? (void)0 : __builtin_trap())

// Static assert - compile-time assertion
#define static_assert(expr, msg) _Static_assert(expr, msg)

// Unreachable - marks code path as unreachable (optimization + runtime trap)
#define unreachable() __builtin_unreachable()

#else
#define assert(expr) __error("assert not implemented for platform")
#define static_assert(expr, msg) __error("static_assert not implemented for platform")
#define unreachable() __error("unreachable not implemented for platform")
#endif

#endif // ASSERT_H
