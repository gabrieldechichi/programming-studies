#ifndef H_INPUT
#define H_INPUT
#include "lib/typedefs.h"
#include "cglm/types.h"

#define INPUT_BUTTONS                                                          \
  INPUT_BUTTON(KEY_A, "A"), INPUT_BUTTON(KEY_D, "D"),                          \
      INPUT_BUTTON(KEY_W, "W"), INPUT_BUTTON(KEY_S, "S"),                      \
      INPUT_BUTTON(KEY_SPACE, "Space"),                                        \
      INPUT_BUTTON(MOUSE_LEFT, "Mouse Left"),                                  \
      INPUT_BUTTON(MOUSE_RIGHT, "Mouse Right"),                                \
      INPUT_BUTTON(MOUSE_MIDDLE, "Mouse Middle")

typedef enum {
#define INPUT_BUTTON(v, a) v
  INPUT_BUTTONS,
  KEY_MAX
#undef INPUT_BUTTON
} Game_InputButtonType;

extern const char *input_button_names[KEY_MAX];

#define EVENT_TYPES                                                            \
  EVENT_TYPE(EVENT_KEYDOWN, "key down"), EVENT_TYPE(EVENT_KEYUP, "key up"),    \
      EVENT_TYPE(EVENT_TOUCH_START, "touch start"),                            \
      EVENT_TYPE(EVENT_TOUCH_END, "touch end"),                                \
      EVENT_TYPE(EVENT_TOUCH_MOVE, "touch move"),                              \
      EVENT_TYPE(EVENT_SCROLL, "scroll")

typedef enum {
#define EVENT_TYPE(v, a) v
  EVENT_TYPES,
  EVENT_MAX
#undef EVENT_TYPE
} Game_InputEventType;

extern const char *input_event_names[EVENT_MAX];

typedef struct {
  Game_InputEventType type;
  union {
    struct {
      Game_InputButtonType type;
    } key;
    struct {
      u32 id;
      f32 x;
      f32 y;
    } touch;
    struct {
      f32 delta_x;
      f32 delta_y;
    } scroll;
  };

} GameInputEvent;

#define GAME_INPUT_EVENTS_MAX_COUNT 20

typedef struct {
  f32 mouse_x;
  f32 mouse_y;
  uint32 len;
  GameInputEvent events[GAME_INPUT_EVENTS_MAX_COUNT];
} GameInputEvents;

typedef struct {
  bool32 is_pressed;
  bool32 pressed_this_frame;
  bool32 released_this_frame;
} GameButton;

typedef struct {
  u32 id;
  bool32 is_active;
  bool32 started_this_frame;
  bool32 stopped_this_frame;
  f32 start_time;
  f32 start_x;
  f32 start_y;
  f32 current_x;
  f32 current_y;
  f32 prev_frame_x;
  f32 prev_frame_y;
} MobileTouch;

#define MAX_TOUCHES 4

typedef struct {
  u32 len;
  MobileTouch items[MAX_TOUCHES];
} MobileTouches;

typedef struct {
  vec2 mouse_pos;
  vec2 mouse_delta;
  vec2 scroll_delta;
  union {
    struct {
      GameButton left;
      GameButton right;
      GameButton up;
      GameButton down;
      GameButton space;
      GameButton mouse_left;
      GameButton mouse_right;
      GameButton mouse_middle;
    };
    GameButton buttons[8];
  };
  MobileTouches touches;
#if GAME_DEBUG
  i32 _frame_update_and_end_stack;
#endif
} GameInput;

GameInput input_init();
void input_update(GameInput *input, GameInputEvents *input_events, f32 now);
void input_update_button(GameButton *btn, GameInputEvent event);
void input_update_touch(MobileTouches* touches, GameInputEvent event, f32 time);
void input_end_frame(GameInput *inputs);
#endif
