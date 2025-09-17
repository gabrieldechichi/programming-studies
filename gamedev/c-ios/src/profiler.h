#pragma once

#include "memory.h"
#include "typedefs.h"

#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 0
#endif

#if PROFILER_ENABLED

typedef struct ProfileAnchor {
    u64 tsc_elapsed_exclusive;  // Does NOT include children
    u64 tsc_elapsed_inclusive;  // DOES include children
    u64 hit_count;
    const char* label;
} ProfileAnchor;

#define PROFILER_MAX_ANCHORS 4096
#define PROFILER_MAX_STACK_DEPTH 256
#define PROFILER_MAX_THREADS 16

// Thread-local profiler state
extern _Thread_local ProfileAnchor g_profiler_anchors[PROFILER_MAX_ANCHORS];
extern _Thread_local u32 g_profiler_parent;
extern _Thread_local int g_thread_registered;

// Global registry for merging thread results
extern ProfileAnchor* g_all_thread_anchors[PROFILER_MAX_THREADS];
extern _Atomic(int) g_thread_count;

typedef struct ProfileBlock {
    const char* label;
    u64 old_tsc_elapsed_inclusive;
    u64 start_tsc;
    u32 parent_index;
    u32 anchor_index;
} ProfileBlock;

extern _Thread_local ProfileBlock* g_profile_stack[PROFILER_MAX_STACK_DEPTH];
extern _Thread_local u32 g_profile_stack_depth;

void profiler_begin_block(ProfileBlock* block, const char* label, u32 anchor_index);
void profiler_end_block(ProfileBlock* block);

#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b) CONCAT_INTERNAL(a, b)

#define PROFILE_BLOCK_VAR(line) CONCAT(prof_block_, line)

#define PROFILE_BEGIN(name) do { \
    static ProfileBlock PROFILE_BLOCK_VAR(__LINE__); \
    g_profile_stack[g_profile_stack_depth++] = &PROFILE_BLOCK_VAR(__LINE__); \
    profiler_begin_block(&PROFILE_BLOCK_VAR(__LINE__), name, __COUNTER__ + 1); \
} while(0)

#define PROFILE_END() do { \
    profiler_end_block(g_profile_stack[--g_profile_stack_depth]); \
} while(0)

#define PROFILE_SCOPE(name) \
    static ProfileBlock PROFILE_BLOCK_VAR(__LINE__); \
    profiler_begin_block(&PROFILE_BLOCK_VAR(__LINE__), name, __COUNTER__ + 1); \
    for (int _prof_done_ = 0; !_prof_done_; _prof_done_ = 1, profiler_end_block(&PROFILE_BLOCK_VAR(__LINE__)))

#define PROFILE_ASSERT_END_OF_COMPILATION_UNIT _Static_assert(__COUNTER__ < PROFILER_MAX_ANCHORS, "Number of profile points exceeds size of profiler anchors array")

void profiler_end_and_print_session(Allocator* allocator);

#else

#define PROFILE_BEGIN(name)
#define PROFILE_END()
#define PROFILE_SCOPE(name)
#define TimeBlock(name)
#define TimeFunction
#define PROFILE_ASSERT_END_OF_COMPILATION_UNIT

static inline void profiler_end_and_print_session(void) {}

#endif
