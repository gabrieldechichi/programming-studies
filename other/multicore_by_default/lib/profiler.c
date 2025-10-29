#include "profiler.h"
#include "os/os.h"
#include <stdlib.h>
#include <stdio.h>

#if PROFILER_ENABLED

#include "string_builder.h"


// Thread-local profiler state
thread_local ProfileAnchor g_profiler_anchors[PROFILER_MAX_ANCHORS] = {0};
thread_local u32 g_profiler_parent = 0;
thread_local ProfileBlock *g_profile_stack[PROFILER_MAX_STACK_DEPTH] = {0};
thread_local u32 g_profile_stack_depth = 0;
thread_local int g_thread_registered = 0;

// Global registry for merging thread results
ProfileAnchor* g_all_thread_anchors[PROFILER_MAX_THREADS];
atomic_int g_thread_count = 0;

static struct {
  u64 start_tsc;
  u64 end_tsc;
} g_profiler = {0};

void profiler_begin_block(ProfileBlock *block, const char *label,
                          u32 anchor_index) {
  // Register this thread's anchors array if not already registered
  if (!g_thread_registered) {
    int idx = atomic_fetch_add(&g_thread_count, 1);
    if (idx < PROFILER_MAX_THREADS) {
      g_all_thread_anchors[idx] = g_profiler_anchors;
      g_thread_registered = 1;
    }
  }

  block->parent_index = g_profiler_parent;
  block->anchor_index = anchor_index;
  block->label = label;

  ProfileAnchor *anchor = &g_profiler_anchors[anchor_index];
  block->old_tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;

  g_profiler_parent = anchor_index;
  block->start_tsc = os_time_now();
}

void profiler_end_block(ProfileBlock *block) {
  u64 elapsed = os_time_now() - block->start_tsc;
  g_profiler_parent = block->parent_index;

  ProfileAnchor *parent = &g_profiler_anchors[block->parent_index];
  ProfileAnchor *anchor = &g_profiler_anchors[block->anchor_index];

  parent->tsc_elapsed_exclusive -= elapsed;
  anchor->tsc_elapsed_exclusive += elapsed;
  anchor->tsc_elapsed_inclusive = block->old_tsc_elapsed_inclusive + elapsed;
  anchor->hit_count++;

  // This write happens every time (as Casey notes, unavoidable in C)
  anchor->label = block->label;
}

void profiler_begin_session(void) {
  g_profiler.start_tsc = os_time_now();

  // Reset thread registration system
  atomic_store(&g_thread_count, 0);

  // Clear all thread anchor pointers
  for (int t = 0; t < PROFILER_MAX_THREADS; t++) {
    g_all_thread_anchors[t] = NULL;
  }

  // Reset current thread's registration flag so it re-registers
  g_thread_registered = 0;

  // Clear current thread's profiler anchors
  for (u32 a = 0; a < PROFILER_MAX_ANCHORS; a++) {
    g_profiler_anchors[a].tsc_elapsed_exclusive = 0;
    g_profiler_anchors[a].tsc_elapsed_inclusive = 0;
    g_profiler_anchors[a].hit_count = 0;
    // Keep the label pointer as it's static
  }
}

static void print_time_elapsed(StringBuilder *sb, u64 total_tsc_elapsed,
                               ProfileAnchor *anchor) {
  f64 exclusive_us = os_ticks_to_ms(anchor->tsc_elapsed_exclusive);
  f64 inclusive_us = os_ticks_to_ms(anchor->tsc_elapsed_inclusive);
  f64 avg_exclusive_us = exclusive_us / (f64)anchor->hit_count;
  f64 avg_inclusive_us = inclusive_us / (f64)anchor->hit_count;
  f64 percent_exclusive =
      100.0 * ((f64)anchor->tsc_elapsed_exclusive / (f64)total_tsc_elapsed);
  f64 percent_inclusive =
      100.0 * ((f64)anchor->tsc_elapsed_inclusive / (f64)total_tsc_elapsed);

  sb_append(sb, "  ");
  sb_append(sb, anchor->label);
  sb_append(sb, ":\n");

  sb_append(sb, "    Hits: ");
  sb_append_u32(sb, anchor->hit_count);

  sb_append(sb, " | Total: ");
  sb_append_f32(sb, exclusive_us, 3);
  sb_append(sb, "ms (");
  sb_append_f32(sb, percent_exclusive, 1);
  sb_append(sb, "%)");

  sb_append(sb, " | Avg: ");
  sb_append_f32(sb, avg_exclusive_us, 3);
  sb_append(sb, "ms");

  if (anchor->tsc_elapsed_inclusive != anchor->tsc_elapsed_exclusive) {
    sb_append(sb, "\n    With children - Total: ");
    sb_append_f32(sb, inclusive_us, 3);
    sb_append(sb, "ms (");
    sb_append_f32(sb, percent_inclusive, 1);
    sb_append(sb, "%)");

    sb_append(sb, " | Avg: ");
    sb_append_f32(sb, avg_inclusive_us, 3);
    sb_append(sb, "ms");
  }

  sb_append(sb, "\n");
}

void profiler_end_and_print_session(Allocator *allocator) {
  g_profiler.end_tsc = os_time_now();

  u64 total_tsc_elapsed = g_profiler.end_tsc - g_profiler.start_tsc;
  f64 total_us = os_ticks_to_ms(total_tsc_elapsed);

  typedef struct {
    ProfileAnchor *anchor;
    f64 sort_time_us;  // Time to sort by (inclusive if has children, exclusive otherwise)
    b32 has_children;
  } SortedAnchor;

  SortedAnchor sorted_anchors[PROFILER_MAX_ANCHORS];
  u32 active_count = 0;

  // First, merge all thread data into a single array
  ProfileAnchor merged_anchors[PROFILER_MAX_ANCHORS] = {0};

  int thread_count = atomic_load(&g_thread_count);
  for (int t = 0; t < thread_count; t++) {
    ProfileAnchor* thread_anchors = g_all_thread_anchors[t];
    for (u32 a = 0; a < PROFILER_MAX_ANCHORS; a++) {
      if (thread_anchors[a].hit_count > 0) {
        merged_anchors[a].tsc_elapsed_exclusive += thread_anchors[a].tsc_elapsed_exclusive;
        merged_anchors[a].tsc_elapsed_inclusive += thread_anchors[a].tsc_elapsed_inclusive;
        merged_anchors[a].hit_count += thread_anchors[a].hit_count;
        merged_anchors[a].label = thread_anchors[a].label;
      }
    }
  }

  // Now process the merged data
  u64 total_profiled_tsc = 0;
  for (u32 anchor_index = 0; anchor_index < PROFILER_MAX_ANCHORS;
       anchor_index++) {
    ProfileAnchor *anchor = &merged_anchors[anchor_index];
    if (anchor->tsc_elapsed_inclusive) {
      if (anchor_index == 0 ||
          anchor->tsc_elapsed_inclusive > total_profiled_tsc) {
        total_profiled_tsc = anchor->tsc_elapsed_inclusive;
      }

      // Determine if this anchor has children
      b32 has_children = (anchor->tsc_elapsed_inclusive > anchor->tsc_elapsed_exclusive);

      // Choose sort metric based on whether it has children
      f64 sort_time;
      if (has_children) {
        // Sort by inclusive time (total time including children)
        sort_time = (f64)anchor->tsc_elapsed_inclusive / (f64)anchor->hit_count;
      } else {
        // Sort by exclusive time (just this function)
        sort_time = (f64)anchor->tsc_elapsed_exclusive / (f64)anchor->hit_count;
      }

      sorted_anchors[active_count].anchor = anchor;
      sorted_anchors[active_count].sort_time_us = sort_time;
      sorted_anchors[active_count].has_children = has_children;
      active_count++;
    }
  }

  // Bubble sort by the hybrid metric
  for (u32 i = 0; i < active_count - 1; i++) {
    for (u32 j = 0; j < active_count - i - 1; j++) {
      if (sorted_anchors[j].sort_time_us <
          sorted_anchors[j + 1].sort_time_us) {
        SortedAnchor temp = sorted_anchors[j];
        sorted_anchors[j] = sorted_anchors[j + 1];
        sorted_anchors[j + 1] = temp;
      }
    }
  }

  size_t buffer_size = MB(10);
  char *buffer = allocator ? ALLOC_ARRAY(allocator, char, buffer_size) : (char*)malloc(buffer_size);
  assert(buffer);
  StringBuilder sb;
  sb_init(&sb, buffer, buffer_size);

  sb_append(&sb, "\n========== PROFILER RESULTS ==========\n");
  sb_append(&sb, "Total session time: ");
  sb_append_f32(&sb, total_us, 4);
  sb_append(&sb, "ms\n");
  sb_append(&sb, "Total profiled time: ");
  sb_append_f32(&sb, os_ticks_to_ms(total_profiled_tsc), 4);
  sb_append(&sb, "ms\n");
  sb_append(&sb, "--------------------------------------\n");
  sb_append(&sb, "(Sorted by inclusive time if has children,\n");
  sb_append(&sb, " otherwise by exclusive time)\n");
  sb_append(&sb, "--------------------------------------\n");

  for (u32 i = 0; i < active_count; i++) {
    print_time_elapsed(&sb, total_profiled_tsc, sorted_anchors[i].anchor);
  }

  sb_append(&sb, "======================================\n");

  printf("%s", sb_get(&sb));

  // Free buffer if we allocated it with malloc
  if (!allocator) {
    free(buffer);
  }
}

#endif
