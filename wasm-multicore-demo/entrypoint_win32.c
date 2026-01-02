#include "context.h"
#include "lib/thread_context.h"
#include "os/os.h"
#include "app.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#include <stdio.h>

// Force discrete GPU on laptops
__declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01;

global Barrier frame_barrier;
global ThreadContext main_thread_ctx;
global AppMemory *g_memory;
global AppContext g_app_ctx;
global b32 g_running;
global HWND g_hwnd;
global b32 g_mouse_locked;
global b32 g_prev_mouse_buttons[3];
global POINT g_mouse_lock_center;
global b32 g_fullscreen;
global WINDOWPLACEMENT g_windowed_placement;
global DWORD g_windowed_style;

typedef struct {
    ThreadContext *ctx;
} WorkerData;

internal void worker_loop(void *arg) {
    WorkerData *data = (WorkerData *)arg;
    tctx_set_current(data->ctx);

    lane_sync();
    app_init(g_memory);
    arena_reset(&tctx_current()->temp_arena);
    lane_sync();

    while (g_running) {
        lane_sync();
        app_update_and_render(g_memory);
        arena_reset(&tctx_current()->temp_arena);
        lane_sync();
    }
}

internal App_InputButtonType vk_to_input_button(WPARAM vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return KEY_A + (App_InputButtonType)(vk - 'A');
    }
    if (vk >= '0' && vk <= '9') {
        return KEY_0 + (App_InputButtonType)(vk - '0');
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return KEY_F1 + (App_InputButtonType)(vk - VK_F1);
    }

    switch (vk) {
        case VK_UP:      return KEY_UP;
        case VK_DOWN:    return KEY_DOWN;
        case VK_LEFT:    return KEY_LEFT;
        case VK_RIGHT:   return KEY_RIGHT;
        case VK_SPACE:   return KEY_SPACE;
        case VK_RETURN:  return KEY_ENTER;
        case VK_ESCAPE:  return KEY_ESCAPE;
        case VK_TAB:     return KEY_TAB;
        case VK_BACK:    return KEY_BACKSPACE;
        case VK_DELETE:  return KEY_DELETE;
        case VK_INSERT:  return KEY_INSERT;
        case VK_HOME:    return KEY_HOME;
        case VK_END:     return KEY_END;
        case VK_PRIOR:   return KEY_PAGE_UP;
        case VK_NEXT:    return KEY_PAGE_DOWN;
        case VK_LSHIFT:  return KEY_LEFT_SHIFT;
        case VK_RSHIFT:  return KEY_RIGHT_SHIFT;
        case VK_LCONTROL: return KEY_LEFT_CONTROL;
        case VK_RCONTROL: return KEY_RIGHT_CONTROL;
        case VK_LMENU:   return KEY_LEFT_ALT;
        case VK_RMENU:   return KEY_RIGHT_ALT;
        default:         return KEY_MAX;
    }
}

internal void win32_add_key_event(AppInputEvents *events, App_InputButtonType key, b32 is_down) {
    if (key == KEY_MAX || events->len >= GAME_INPUT_EVENTS_MAX_COUNT) return;

    AppInputEvent *ev = &events->events[events->len++];
    ev->type = is_down ? INPUT_EVENT_KEYDOWN : INPUT_EVENT_KEYUP;
    ev->key.type = key;
}

internal void win32_process_pending_messages(AppInputEvents *events) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_running = false;
            break;
        }

        switch (msg.message) {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                WPARAM vk = msg.wParam;
                b32 was_down = (msg.lParam & (1 << 30)) != 0;
                b32 is_down = (msg.lParam & (1UL << 31)) == 0;

                if (was_down != is_down) {
                    App_InputButtonType key = vk_to_input_button(vk);
                    win32_add_key_event(events, key, is_down);

                    if (vk == VK_F4 && (msg.lParam & (1 << 29))) {
                        g_running = false;
                    }
                }
            } break;

            case WM_CHAR: {
                if (events->len < GAME_INPUT_EVENTS_MAX_COUNT) {
                    AppInputEvent *ev = &events->events[events->len++];
                    ev->type = INPUT_EVENT_CHAR;
                    ev->character.codepoint = (u32)msg.wParam;
                }
            } break;

            case WM_MOUSEWHEEL: {
                if (events->len < GAME_INPUT_EVENTS_MAX_COUNT) {
                    i16 wheel_delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
                    AppInputEvent *ev = &events->events[events->len++];
                    ev->type = INPUT_EVENT_SCROLL;
                    ev->scroll.delta_x = 0;
                    ev->scroll.delta_y = (f32)wheel_delta / (f32)WHEEL_DELTA;
                }
            } break;

            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }
}

internal void win32_poll_mouse(AppInputEvents *events, HWND hwnd, f32 dpr) {
    POINT mouse_p;
    GetCursorPos(&mouse_p);

    if (g_mouse_locked) {
        f32 dx = (f32)(mouse_p.x - g_mouse_lock_center.x) / dpr;
        f32 dy = (f32)(mouse_p.y - g_mouse_lock_center.y) / dpr;
        events->mouse_x += dx;
        events->mouse_y += dy;
        SetCursorPos(g_mouse_lock_center.x, g_mouse_lock_center.y);
    } else {
        ScreenToClient(hwnd, &mouse_p);
        events->mouse_x = (f32)mouse_p.x / dpr;
        events->mouse_y = (f32)mouse_p.y / dpr;
    }

    struct { int vk; App_InputButtonType btn; } mouse_buttons[] = {
        { VK_LBUTTON, MOUSE_LEFT },
        { VK_RBUTTON, MOUSE_RIGHT },
        { VK_MBUTTON, MOUSE_MIDDLE },
    };

    for (u32 i = 0; i < ARRAY_SIZE(mouse_buttons); i++) {
        b32 is_down = (GetKeyState(mouse_buttons[i].vk) & (1 << 15)) != 0;
        if (is_down != g_prev_mouse_buttons[i]) {
            win32_add_key_event(events, mouse_buttons[i].btn, is_down);
            g_prev_mouse_buttons[i] = is_down;
        }
    }
}

internal LRESULT CALLBACK win32_window_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            if (g_memory) {
                RECT client;
                GetClientRect(hwnd, &client);
                g_memory->canvas_width = (f32)(client.right - client.left);
                g_memory->canvas_height = (f32)(client.bottom - client.top);
            }
        } break;

        case WM_DPICHANGED: {
            if (g_memory) {
                u32 dpi = HIWORD(wparam);
                g_memory->dpr = (f32)dpi / 96.0f;

                RECT *suggested = (RECT *)lparam;
                SetWindowPos(hwnd, NULL,
                    suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        } break;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

internal HWND win32_create_window(HINSTANCE instance, i32 width, i32 height) {
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = win32_window_callback;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "WasmMulticoreWindowClass";

    if (!RegisterClassA(&wc)) {
        LOG_ERROR("Failed to register window class");
        return NULL;
    }

    RECT window_rect = { 0, 0, width, height };
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Wasm Multicore Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL, NULL, instance, NULL
    );

    if (!hwnd) {
        LOG_ERROR("Failed to create window");
        return NULL;
    }

    return hwnd;
}

void os_quit(void) {
    g_running = false;
    PostQuitMessage(0);
}

void os_lock_mouse(b32 lock) {
    if (lock == g_mouse_locked) return;

    g_mouse_locked = lock;
    if (lock) {
        RECT client_rect;
        GetClientRect(g_hwnd, &client_rect);
        POINT center = {
            (client_rect.right - client_rect.left) / 2,
            (client_rect.bottom - client_rect.top) / 2
        };
        ClientToScreen(g_hwnd, &center);
        g_mouse_lock_center = center;
        SetCursorPos(center.x, center.y);

        RECT clip_rect;
        GetWindowRect(g_hwnd, &clip_rect);
        ClipCursor(&clip_rect);
        ShowCursor(FALSE);
    } else {
        ClipCursor(NULL);
        ShowCursor(TRUE);
    }
}

b32 os_is_mouse_locked(void) {
    return g_mouse_locked;
}

void os_set_fullscreen(b32 fullscreen) {
    if (fullscreen == g_fullscreen) return;

    if (fullscreen) {
        g_windowed_placement.length = sizeof(g_windowed_placement);
        GetWindowPlacement(g_hwnd, &g_windowed_placement);
        g_windowed_style = GetWindowLongA(g_hwnd, GWL_STYLE);

        MONITORINFO mi = { .cbSize = sizeof(mi) };
        if (GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongA(g_hwnd, GWL_STYLE, g_windowed_style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(g_hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLongA(g_hwnd, GWL_STYLE, g_windowed_style);
        SetWindowPlacement(g_hwnd, &g_windowed_placement);
        SetWindowPos(g_hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    g_fullscreen = fullscreen;
}

b32 os_is_fullscreen(void) {
    return g_fullscreen;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    UNUSED(prev_instance);
    UNUSED(cmd_line);

    // Attach console for debug output
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE *dummy;
        freopen_s(&dummy, "CON", "w", stdout);
        freopen_s(&dummy, "CON", "w", stderr);
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    UINT desired_scheduler_ms = 1;
    b32 sleep_is_granular = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);
    UNUSED(sleep_is_granular);

    os_init();
    os_time_init();
    os_install_crash_handler();

    i32 initial_width = 1280;
    i32 initial_height = 720;

    g_hwnd = win32_create_window(instance, initial_width, initial_height);
    if (!g_hwnd) {
        return 1;
    }

    u32 dpi = GetDpiForWindow(g_hwnd);
    f32 dpr = (f32)dpi / 96.0f;

    const size_t heap_size = GB(2);
    u8 *heap = os_allocate_memory(heap_size);
    if (!heap) {
        LOG_ERROR("Failed to allocate heap memory");
        return 1;
    }

    local_persist AppMemory memory = {0};
    memory.heap = heap;
    memory.heap_size = heap_size;
    memory.canvas_width = (f32)initial_width;
    memory.canvas_height = (f32)initial_height;
    memory.dpr = dpr;
    g_memory = &memory;

    g_app_ctx.arena = arena_from_buffer(heap, heap_size);
    g_app_ctx.num_threads = (u8)os_get_processor_count();
    app_ctx_set(&g_app_ctx);

    LOG_INFO("Starting with % threads", FMT_UINT(g_app_ctx.num_threads));

    Thread *threads = ARENA_ALLOC_ARRAY(&g_app_ctx.arena, Thread, g_app_ctx.num_threads);
    ThreadContext *thread_contexts = ARENA_ALLOC_ARRAY(&g_app_ctx.arena, ThreadContext, g_app_ctx.num_threads);
    WorkerData *worker_data = ARENA_ALLOC_ARRAY(&g_app_ctx.arena, WorkerData, g_app_ctx.num_threads);

    frame_barrier = barrier_alloc(g_app_ctx.num_threads);

    main_thread_ctx = (ThreadContext){
        .thread_idx = 0,
        .thread_count = g_app_ctx.num_threads,
        .barrier = &frame_barrier,
        .temp_arena = arena_from_buffer(
            ARENA_ALLOC_ARRAY(&g_app_ctx.arena, u8, MB(16)), MB(16)),
    };
    tctx_set_current(&main_thread_ctx);

    for (u8 i = 1; i < g_app_ctx.num_threads; i++) {
        thread_contexts[i] = (ThreadContext){
            .thread_idx = i,
            .thread_count = g_app_ctx.num_threads,
            .barrier = &frame_barrier,
            .temp_arena = arena_from_buffer(
                ARENA_ALLOC_ARRAY(&g_app_ctx.arena, u8, MB(16)), MB(16)),
        };
        worker_data[i] = (WorkerData){ .ctx = &thread_contexts[i] };
        threads[i] = thread_launch(worker_loop, &worker_data[i]);
    }

    g_running = true;

    lane_sync();
    app_init(&memory);
    arena_reset(&tctx_current()->temp_arena);
    lane_sync();

    ShowWindow(g_hwnd, show_cmd);
    UpdateWindow(g_hwnd);

    u64 last_time = os_time_now();
    f32 target_frame_time_ms = 16.667f;

    while (g_running) {
        u64 frame_start = os_time_now();
        f32 dt = NS_TO_SECS(os_time_diff(frame_start, last_time));
        last_time = frame_start;

        memory.dt = dt;
        memory.total_time += dt;

        memory.input_events.len = 0;
        win32_process_pending_messages(&memory.input_events);
        win32_poll_mouse(&memory.input_events, g_hwnd, memory.dpr);

        if (!g_running) break;

        lane_sync();
        app_update_and_render(&memory);
        arena_reset(&tctx_current()->temp_arena);
        lane_sync();

        u64 frame_end = os_time_now();
        f32 frame_time_ms = (f32)NS_TO_MS(os_time_diff(frame_end, frame_start));
        if (frame_time_ms < target_frame_time_ms) {
            f32 sleep_time_ms = target_frame_time_ms - frame_time_ms;
            if (sleep_time_ms > 1.0f) {
                os_sleep((u64)((sleep_time_ms - 1.0f) * 1000.0f));
            }
            while ((f32)NS_TO_MS(os_time_diff(os_time_now(), frame_start)) < target_frame_time_ms) {
                // spin
            }
        }
    }

    g_running = false;
    lane_sync();
    lane_sync();

    DestroyWindow(g_hwnd);
    timeEndPeriod(desired_scheduler_ms);

    return 0;
}
