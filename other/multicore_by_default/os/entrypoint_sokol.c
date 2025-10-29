#include "app/app.h"
#include "lib/fmt.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "renderer/renderer.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"
#include <stdlib.h>
#ifndef WIN64
#include <unistd.h>
#endif
#include "lib/profiler.h"

#ifdef HOT_RELOAD
#include "os/hotreload.h"

// Hot reload tracking
#ifdef _WIN32
static const char *game_dylib_path = "out/windowsdll/game.dll";
static const char *game_dylib_temp_path = "out/windowsdll/game_temp.dll";
#else
static const char *game_dylib_path = "out/macosdll/game.dylib";
static const char *game_dylib_temp_path = "out/macosdll/game_temp.dylib";
#endif

#else
// Static linking - direct function declarations
extern void app_init(AppMemory *);
extern void app_update_and_render(AppMemory *);
extern void app_on_reload(AppMemory *);
#endif

#define PERMANENT_MEMORY_SIZE GB(4)
#define TEMPORARY_MEMORY_SIZE MB(256)

typedef struct {
  AppMemory game_memory;
  AppInputEvents input_events_buffer;
  f32 last_now;
#ifdef HOT_RELOAD
  HotReloadAppCode app_code;
#endif
} Entrypoint_Sokol;

internal Entrypoint_Sokol sokol;

internal App_InputButtonType sokol_keycode_to_game_button(sapp_keycode key) {
  switch (key) {
  case SAPP_KEYCODE_A: return KEY_A;
  case SAPP_KEYCODE_B: return KEY_B;
  case SAPP_KEYCODE_C: return KEY_C;
  case SAPP_KEYCODE_D: return KEY_D;
  case SAPP_KEYCODE_E: return KEY_E;
  case SAPP_KEYCODE_F: return KEY_F;
  case SAPP_KEYCODE_G: return KEY_G;
  case SAPP_KEYCODE_H: return KEY_H;
  case SAPP_KEYCODE_I: return KEY_I;
  case SAPP_KEYCODE_J: return KEY_J;
  case SAPP_KEYCODE_K: return KEY_K;
  case SAPP_KEYCODE_L: return KEY_L;
  case SAPP_KEYCODE_M: return KEY_M;
  case SAPP_KEYCODE_N: return KEY_N;
  case SAPP_KEYCODE_O: return KEY_O;
  case SAPP_KEYCODE_P: return KEY_P;
  case SAPP_KEYCODE_Q: return KEY_Q;
  case SAPP_KEYCODE_R: return KEY_R;
  case SAPP_KEYCODE_S: return KEY_S;
  case SAPP_KEYCODE_T: return KEY_T;
  case SAPP_KEYCODE_U: return KEY_U;
  case SAPP_KEYCODE_V: return KEY_V;
  case SAPP_KEYCODE_W: return KEY_W;
  case SAPP_KEYCODE_X: return KEY_X;
  case SAPP_KEYCODE_Y: return KEY_Y;
  case SAPP_KEYCODE_Z: return KEY_Z;
  case SAPP_KEYCODE_0: return KEY_0;
  case SAPP_KEYCODE_1: return KEY_1;
  case SAPP_KEYCODE_2: return KEY_2;
  case SAPP_KEYCODE_3: return KEY_3;
  case SAPP_KEYCODE_4: return KEY_4;
  case SAPP_KEYCODE_5: return KEY_5;
  case SAPP_KEYCODE_6: return KEY_6;
  case SAPP_KEYCODE_7: return KEY_7;
  case SAPP_KEYCODE_8: return KEY_8;
  case SAPP_KEYCODE_9: return KEY_9;
  case SAPP_KEYCODE_F1: return KEY_F1;
  case SAPP_KEYCODE_F2: return KEY_F2;
  case SAPP_KEYCODE_F3: return KEY_F3;
  case SAPP_KEYCODE_F4: return KEY_F4;
  case SAPP_KEYCODE_F5: return KEY_F5;
  case SAPP_KEYCODE_F6: return KEY_F6;
  case SAPP_KEYCODE_F7: return KEY_F7;
  case SAPP_KEYCODE_F8: return KEY_F8;
  case SAPP_KEYCODE_F9: return KEY_F9;
  case SAPP_KEYCODE_F10: return KEY_F10;
  case SAPP_KEYCODE_F11: return KEY_F11;
  case SAPP_KEYCODE_F12: return KEY_F12;
  case SAPP_KEYCODE_UP: return KEY_UP;
  case SAPP_KEYCODE_DOWN: return KEY_DOWN;
  case SAPP_KEYCODE_LEFT: return KEY_LEFT;
  case SAPP_KEYCODE_RIGHT: return KEY_RIGHT;
  case SAPP_KEYCODE_SPACE: return KEY_SPACE;
  case SAPP_KEYCODE_ENTER: return KEY_ENTER;
  case SAPP_KEYCODE_ESCAPE: return KEY_ESCAPE;
  case SAPP_KEYCODE_TAB: return KEY_TAB;
  case SAPP_KEYCODE_BACKSPACE: return KEY_BACKSPACE;
  case SAPP_KEYCODE_DELETE: return KEY_DELETE;
  case SAPP_KEYCODE_INSERT: return KEY_INSERT;
  case SAPP_KEYCODE_HOME: return KEY_HOME;
  case SAPP_KEYCODE_END: return KEY_END;
  case SAPP_KEYCODE_PAGE_UP: return KEY_PAGE_UP;
  case SAPP_KEYCODE_PAGE_DOWN: return KEY_PAGE_DOWN;
  case SAPP_KEYCODE_LEFT_SHIFT: return KEY_LEFT_SHIFT;
  case SAPP_KEYCODE_RIGHT_SHIFT: return KEY_RIGHT_SHIFT;
  case SAPP_KEYCODE_LEFT_CONTROL: return KEY_LEFT_CONTROL;
  case SAPP_KEYCODE_RIGHT_CONTROL: return KEY_RIGHT_CONTROL;
  case SAPP_KEYCODE_LEFT_ALT: return KEY_LEFT_ALT;
  case SAPP_KEYCODE_RIGHT_ALT: return KEY_RIGHT_ALT;
  default:
    return KEY_MAX;
  }
}

internal void add_input_event(AppInputEvent event) {
  if (sokol.input_events_buffer.len < GAME_INPUT_EVENTS_MAX_COUNT) {
    sokol.input_events_buffer.events[sokol.input_events_buffer.len++] = event;
  }
}

internal void sokol_event(const sapp_event *e) {
  switch (e->type) {
  case SAPP_EVENTTYPE_KEY_DOWN: {
    App_InputButtonType btn = sokol_keycode_to_game_button(e->key_code);
    if (btn != KEY_MAX) {
      AppInputEvent event = {0};
      event.type = INPUT_EVENT_KEYDOWN;
      event.key.type = btn;
      add_input_event(event);
    }
    break;
  }
  case SAPP_EVENTTYPE_KEY_UP: {
    App_InputButtonType btn = sokol_keycode_to_game_button(e->key_code);
    if (btn != KEY_MAX) {
      AppInputEvent event = {0};
      event.type = INPUT_EVENT_KEYUP;
      event.key.type = btn;
      add_input_event(event);
    }
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_DOWN: {
    AppInputEvent event = {0};
    event.type = INPUT_EVENT_KEYDOWN;
    switch (e->mouse_button) {
    case SAPP_MOUSEBUTTON_LEFT:
      event.key.type = MOUSE_LEFT;
      break;
    case SAPP_MOUSEBUTTON_RIGHT:
      event.key.type = MOUSE_RIGHT;
      break;
    case SAPP_MOUSEBUTTON_MIDDLE:
      event.key.type = MOUSE_MIDDLE;
      break;
    default:
      return; // Unknown mouse button
    }
    add_input_event(event);
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_UP: {
    AppInputEvent event = {0};
    event.type = INPUT_EVENT_KEYUP;
    switch (e->mouse_button) {
    case SAPP_MOUSEBUTTON_LEFT:
      event.key.type = MOUSE_LEFT;
      break;
    case SAPP_MOUSEBUTTON_RIGHT:
      event.key.type = MOUSE_RIGHT;
      break;
    case SAPP_MOUSEBUTTON_MIDDLE:
      event.key.type = MOUSE_MIDDLE;
      break;
    default:
      return; // Unknown mouse button
    }

    add_input_event(event);
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    // Update mouse position in the buffer (this gets read each frame)
    sokol.input_events_buffer.mouse_x = e->mouse_x;
    sokol.input_events_buffer.mouse_y = e->mouse_y;
    break;
  }
  case SAPP_EVENTTYPE_TOUCHES_BEGAN: {
    for (int i = 0; i < e->num_touches; i++) {
      AppInputEvent event = {0};
      event.type = INPUT_EVENT_TOUCH_START;
      event.touch.id = (u32)i;
      event.touch.x = e->touches[i].pos_x;
      event.touch.y = e->touches[i].pos_y;
      add_input_event(event);
    }
    break;
  }
  case SAPP_EVENTTYPE_TOUCHES_ENDED: {
    for (int i = 0; i < e->num_touches; i++) {
      AppInputEvent event = {0};
      event.type = INPUT_EVENT_TOUCH_END;
      event.touch.id = (u32)i;
      event.touch.x = e->touches[i].pos_x;
      event.touch.y = e->touches[i].pos_y;
      add_input_event(event);
    }
    break;
  }
  case SAPP_EVENTTYPE_TOUCHES_MOVED: {
    for (int i = 0; i < e->num_touches; i++) {
      AppInputEvent event = {0};
      event.type = INPUT_EVENT_TOUCH_MOVE;
      event.touch.id = (u32)i;
      event.touch.x = e->touches[i].pos_x;
      event.touch.y = e->touches[i].pos_y;
      add_input_event(event);
    }
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_SCROLL: {
    AppInputEvent event = {0};
    event.type = INPUT_EVENT_SCROLL;
    event.scroll.delta_x = e->scroll_x;
    event.scroll.delta_y = e->scroll_y;

    add_input_event(event);
    break;
  }
  case SAPP_EVENTTYPE_RESIZED: {
    // todo: need to send this to game so it can handle resize
  } break;
  default:
    break;
  }
}

void engine_log(const char *tag, uint32_t log_level, uint32_t log_item,
                const char *message, uint32_t line_nr, const char *filename,
                void *user_data) {
    UNUSED(tag);
    UNUSED(log_item);
    UNUSED(user_data);
  const char *log_level_str;
  LogLevel log_level_type;
  switch (log_level) {
  case 0:
    log_level_str = "panic";
    log_level_type = LOGLEVEL_ERROR;
    break;
  case 1:
    log_level_str = "error";
    log_level_type = LOGLEVEL_ERROR;
    break;
  case 2:
    log_level_str = "warning";
    log_level_type = LOGLEVEL_WARN;
    break;
  default:
    log_level_str = "info";
    log_level_type = LOGLEVEL_INFO;
    break;
  }
  PLATFORM_LOG(log_level_type, "[%] %: % - %", FMT_STR(log_level_str),
               FMT_STR(filename), FMT_UINT(line_nr), FMT_STR(message));
  if (log_level == 0) {
    abort();
  }
}

internal void init(void) {
  sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = engine_log,
      .uniform_buffer_size = MB(64),
  });

  os_install_crash_handler();

  os_time_init();

  sokol.game_memory.permanent_memory =
      os_allocate_memory(PERMANENT_MEMORY_SIZE);
  sokol.game_memory.permanent_memory_size = PERMANENT_MEMORY_SIZE;

  sokol.game_memory.temporary_memory =
      os_allocate_memory(TEMPORARY_MEMORY_SIZE);
  sokol.game_memory.temporary_memory_size = TEMPORARY_MEMORY_SIZE;

  if (!sokol.game_memory.permanent_memory ||
      !sokol.game_memory.temporary_memory) {
    LOG_ERROR("Failed to allocate game memory");
    exit(1);
  }

  sokol.game_memory.time.now = 0.0f;
  sokol.game_memory.time.dt = 0.0f;
  sokol.last_now = 0.0f;

  sokol.game_memory.canvas.width = sapp_width();
  sokol.game_memory.canvas.height = sapp_height();

  sokol.game_memory.worker_queue =
      os_work_queue_create(os_get_processor_count());
  if (!sokol.game_memory.worker_queue) {
    LOG_ERROR("Failed to create worker queue");
    exit(1);
  }

#ifdef HOT_RELOAD
  sokol.app_code =
      hotreload_load_game_code(game_dylib_path, game_dylib_temp_path);
  if (!sokol.app_code.is_valid) {
    LOG_ERROR("Failed to load game code");
    exit(1);
  }
  LOG_INFO("Successfully loaded game library");

  if (sokol.app_code.init) {
    sokol.app_code.init(&sokol.game_memory);
  }
#else
  app_init(&sokol.game_memory);
#endif
}

internal void frame(void) {
  PROFILE_BEGIN("frame");

  sokol.last_now = sokol.game_memory.time.now;
  sokol.game_memory.time.now = (f32)os_ticks_to_ms(os_time_now()) / 1000.0f;
  sokol.game_memory.time.dt = sokol.game_memory.time.now - sokol.last_now;

  u32 width = sapp_width();
  u32 height = sapp_height();
  sokol.game_memory.canvas.width = width;
  sokol.game_memory.canvas.height = height;

  sokol.game_memory.input_events = sokol.input_events_buffer;

#ifdef HOT_RELOAD
  if (sokol.app_code.is_valid && sokol.app_code.update_and_render) {
    sokol.app_code.update_and_render(&sokol.game_memory);
  }
#else
  app_update_and_render(&sokol.game_memory);
#endif

  sokol.input_events_buffer.len = 0;

  PROFILE_END();

#ifdef HOT_RELOAD
  // Check for game library changes
  hotreload_check_and_reload(&sokol.app_code, &sokol.game_memory,
                             game_dylib_path, game_dylib_temp_path,
                             sokol.game_memory.time.dt);
#endif
}

internal void cleanup(void) {
#if defined(DEBUG) && defined(PROFILER_ENABLED)
  ArenaAllocator temp_allocator =
      arena_from_buffer(sokol.game_memory.temporary_memory,
                        sokol.game_memory.temporary_memory_size);
  Allocator allocator = make_arena_allocator(&temp_allocator);

  profiler_end_and_print_session(&allocator);
#endif

  //note: we deliberately don't clean memory here, we are exiting the application
}

sapp_desc sokol_main(int argc, char *argv[]) {
  UNUSED(argc);
  UNUSED(argv);
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = sokol_event,
      .sample_count = DISPLAY_SAMPLE_COUNT,
      .window_title = "Kikitora - Demo",
      .width = 1280,
      .height = 720,
      .icon.sokol_default = true,
      .logger.func = slog_func,
      .high_dpi = true,
      .win32_console_attach = true,
      .ios_keyboard_resizes_canvas = false,
  };
}

void os_quit() { sapp_quit(); }
