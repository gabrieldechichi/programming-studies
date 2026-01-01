#include "app.h"
#include "input.h"
#include "lib/typedefs.h"
#include "lib/thread_context.h"

local_shared InputSystem g_input;
local_shared u32 g_frame_count;

void app_init(AppMemory *memory) {
    UNUSED(memory);
    ThreadContext *tctx = tctx_current();

    if (is_main_thread()) {
        g_input = input_init();
        g_frame_count = 0;
        LOG_INFO("Input demo initialized");
    }

    LOG_INFO("Thread % ready", FMT_UINT(tctx->thread_idx));
}

void app_update_and_render(AppMemory *memory) {
    ThreadContext *tctx = tctx_current();
    u8 thread_idx = tctx->thread_idx;

    if (is_main_thread()) {
        input_update(&g_input, &memory->input_events, memory->total_time);
        g_frame_count++;
    }

    lane_sync();

    b32 should_log_state = (g_frame_count % 60) == 0;

    if (should_log_state && is_main_thread()) {
        LOG_INFO("=== INPUT STATE (frame %) ===", FMT_UINT(g_frame_count));
        LOG_INFO("Mouse pos: (%, %) delta: (%, %) scroll: (%, %)",
                 FMT_FLOAT(g_input.mouse_pos[0]),
                 FMT_FLOAT(g_input.mouse_pos[1]),
                 FMT_FLOAT(g_input.mouse_delta[0]),
                 FMT_FLOAT(g_input.mouse_delta[1]),
                 FMT_FLOAT(g_input.scroll_delta[0]),
                 FMT_FLOAT(g_input.scroll_delta[1]));

        u32 pressed_count = 0;
        for (u32 i = 0; i < KEY_MAX; i++) {
            if (g_input.buttons[i].is_pressed) {
                pressed_count++;
            }
        }

        if (pressed_count > 0) {
            LOG_INFO("Pressed keys (%):", FMT_UINT(pressed_count));
            for (u32 i = 0; i < KEY_MAX; i++) {
                if (g_input.buttons[i].is_pressed) {
                    LOG_INFO("  %", FMT_STR(input_button_names[i]));
                }
            }
        }
    }

    for (u32 i = 0; i < KEY_MAX; i++) {
        if (g_input.buttons[i].pressed_this_frame) {
            LOG_INFO("[Thread %] % PRESSED", FMT_UINT(thread_idx), FMT_STR(input_button_names[i]));
        }
        if (g_input.buttons[i].released_this_frame) {
            LOG_INFO("[Thread %] % RELEASED", FMT_UINT(thread_idx), FMT_STR(input_button_names[i]));
        }
    }

    if (is_main_thread()) {
        for (u32 i = 0; i < g_input.chars_len; i++) {
            LOG_INFO("CHAR: '%' (codepoint: %)",
                     FMT_CHAR((char)g_input.chars[i]),
                     FMT_UINT(g_input.chars[i]));
        }
    }

    lane_sync();

    if (is_main_thread()) {
        input_end_frame(&g_input);
    }
}
