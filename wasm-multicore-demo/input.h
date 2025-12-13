/*
    input.h - Input system for keyboard, mouse, and touch

    OVERVIEW

    --- InputSystem tracks button states, mouse position, and touch events

    --- supports pressed_this_frame/released_this_frame for one-shot actions

    --- handles multiple simultaneous touches (mobile)

    USAGE
        InputSystem input = input_init();

        input_update(&input, &input_events, time);

        if (input.buttons[KEY_SPACE].pressed_this_frame) { jump(); }
        if (input.buttons[MOUSE_LEFT].is_pressed) { shoot(); }

        vec2 mouse_delta = input.mouse_delta;

        input_end_frame(&input);
*/

#ifndef H_INPUT
#define H_INPUT
#include "lib/typedefs.h"
#include "cglm/types.h"

#define INPUT_BUTTONS                                                          \
  INPUT_BUTTON(KEY_A, "A"), INPUT_BUTTON(KEY_B, "B"),                          \
      INPUT_BUTTON(KEY_C, "C"), INPUT_BUTTON(KEY_D, "D"),                      \
      INPUT_BUTTON(KEY_E, "E"), INPUT_BUTTON(KEY_F, "F"),                      \
      INPUT_BUTTON(KEY_G, "G"), INPUT_BUTTON(KEY_H, "H"),                      \
      INPUT_BUTTON(KEY_I, "I"), INPUT_BUTTON(KEY_J, "J"),                      \
      INPUT_BUTTON(KEY_K, "K"), INPUT_BUTTON(KEY_L, "L"),                      \
      INPUT_BUTTON(KEY_M, "M"), INPUT_BUTTON(KEY_N, "N"),                      \
      INPUT_BUTTON(KEY_O, "O"), INPUT_BUTTON(KEY_P, "P"),                      \
      INPUT_BUTTON(KEY_Q, "Q"), INPUT_BUTTON(KEY_R, "R"),                      \
      INPUT_BUTTON(KEY_S, "S"), INPUT_BUTTON(KEY_T, "T"),                      \
      INPUT_BUTTON(KEY_U, "U"), INPUT_BUTTON(KEY_V, "V"),                      \
      INPUT_BUTTON(KEY_W, "W"), INPUT_BUTTON(KEY_X, "X"),                      \
      INPUT_BUTTON(KEY_Y, "Y"), INPUT_BUTTON(KEY_Z, "Z"),                      \
      INPUT_BUTTON(KEY_0, "0"), INPUT_BUTTON(KEY_1, "1"),                      \
      INPUT_BUTTON(KEY_2, "2"), INPUT_BUTTON(KEY_3, "3"),                      \
      INPUT_BUTTON(KEY_4, "4"), INPUT_BUTTON(KEY_5, "5"),                      \
      INPUT_BUTTON(KEY_6, "6"), INPUT_BUTTON(KEY_7, "7"),                      \
      INPUT_BUTTON(KEY_8, "8"), INPUT_BUTTON(KEY_9, "9"),                      \
      INPUT_BUTTON(KEY_F1, "F1"), INPUT_BUTTON(KEY_F2, "F2"),                  \
      INPUT_BUTTON(KEY_F3, "F3"), INPUT_BUTTON(KEY_F4, "F4"),                  \
      INPUT_BUTTON(KEY_F5, "F5"), INPUT_BUTTON(KEY_F6, "F6"),                  \
      INPUT_BUTTON(KEY_F7, "F7"), INPUT_BUTTON(KEY_F8, "F8"),                  \
      INPUT_BUTTON(KEY_F9, "F9"), INPUT_BUTTON(KEY_F10, "F10"),                \
      INPUT_BUTTON(KEY_F11, "F11"), INPUT_BUTTON(KEY_F12, "F12"),              \
      INPUT_BUTTON(KEY_UP, "Up"), INPUT_BUTTON(KEY_DOWN, "Down"),              \
      INPUT_BUTTON(KEY_LEFT, "Left"), INPUT_BUTTON(KEY_RIGHT, "Right"),        \
      INPUT_BUTTON(KEY_SPACE, "Space"), INPUT_BUTTON(KEY_ENTER, "Enter"),      \
      INPUT_BUTTON(KEY_ESCAPE, "Escape"), INPUT_BUTTON(KEY_TAB, "Tab"),        \
      INPUT_BUTTON(KEY_BACKSPACE, "Backspace"),                                \
      INPUT_BUTTON(KEY_DELETE, "Delete"), INPUT_BUTTON(KEY_INSERT, "Insert"),  \
      INPUT_BUTTON(KEY_HOME, "Home"), INPUT_BUTTON(KEY_END, "End"),            \
      INPUT_BUTTON(KEY_PAGE_UP, "Page Up"),                                    \
      INPUT_BUTTON(KEY_PAGE_DOWN, "Page Down"),                                \
      INPUT_BUTTON(KEY_LEFT_SHIFT, "Left Shift"),                              \
      INPUT_BUTTON(KEY_RIGHT_SHIFT, "Right Shift"),                            \
      INPUT_BUTTON(KEY_LEFT_CONTROL, "Left Control"),                          \
      INPUT_BUTTON(KEY_RIGHT_CONTROL, "Right Control"),                        \
      INPUT_BUTTON(KEY_LEFT_ALT, "Left Alt"),                                  \
      INPUT_BUTTON(KEY_RIGHT_ALT, "Right Alt"),                                \
      INPUT_BUTTON(MOUSE_LEFT, "Mouse Left"),                                  \
      INPUT_BUTTON(MOUSE_RIGHT, "Mouse Right"),                                \
      INPUT_BUTTON(MOUSE_MIDDLE, "Mouse Middle")

typedef enum {
#define INPUT_BUTTON(v, a) v
  INPUT_BUTTONS,
  KEY_MAX
#undef INPUT_BUTTON
} App_InputButtonType;

global const char *input_button_names[KEY_MAX] = {
#define INPUT_BUTTON(v, a) a
    INPUT_BUTTONS
#undef INPUT_BUTTON
};

#define INPUT_EVENT_TYPES                                                      \
  EVENT_TYPE(INPUT_EVENT_KEYDOWN, "key down"),                                 \
      EVENT_TYPE(INPUT_EVENT_KEYUP, "key up"),                                 \
      EVENT_TYPE(INPUT_EVENT_TOUCH_START, "touch start"),                      \
      EVENT_TYPE(INPUT_EVENT_TOUCH_END, "touch end"),                          \
      EVENT_TYPE(INPUT_EVENT_TOUCH_MOVE, "touch move"),                        \
      EVENT_TYPE(INPUT_EVENT_SCROLL, "scroll"),                                \
      EVENT_TYPE(INPUT_EVENT_CHAR, "char"),                                    \
      EVENT_TYPE(INPUT_EVENT_TEXT_INPUT, "text input")

typedef enum {
#define EVENT_TYPE(v, a) v
  INPUT_EVENT_TYPES,
  INPUT_EVENT_MAX
#undef EVENT_TYPE
} App_InputEventType;

global const char *input_event_names[INPUT_EVENT_MAX] = {
#define EVENT_TYPE(v, a) a
    INPUT_EVENT_TYPES
#undef EVENT_TYPE
};

typedef struct {
  App_InputEventType type;
  union {
    struct {
      App_InputButtonType type;
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
    struct {
      u32 codepoint;
    } character;
    struct {
      char *value;
    } text;
    struct {
      const char *text;
      u32 cursor_pos;
      u32 selection_length;
    } text_input;
  };

} AppInputEvent;

#define GAME_INPUT_EVENTS_MAX_COUNT 20

typedef struct {
  f32 mouse_x;
  f32 mouse_y;
  uint32 len;
  AppInputEvent events[GAME_INPUT_EVENTS_MAX_COUNT];
} AppInputEvents;

/* button state (keyboard/mouse button) */
typedef struct {
  b32 is_pressed;
  b32 pressed_this_frame;
  b32 released_this_frame;
} AppButton;

/* mobile touch state */
typedef struct {
  u32 id;
  b32 is_active;
  b32 started_this_frame;
  b32 stopped_this_frame;
  f32 start_time;
  f32 start_x;
  f32 start_y;
  f32 current_x;
  f32 current_y;
  f32 prev_frame_x;
  f32 prev_frame_y;
} MobileTouch;

#define MAX_TOUCHES 4

/* mobile touches array */
typedef struct {
  u32 len;
  MobileTouch items[MAX_TOUCHES];
} MobileTouches;

#define MAX_CHAR_EVENTS 16
#define MAX_TEXT_INPUT_LEN 1024

/* input system state */
typedef struct {
  vec2 mouse_pos;
  vec2 mouse_delta;
  vec2 scroll_delta;
  AppButton buttons[KEY_MAX];
  MobileTouches touches;
  u32 chars[MAX_CHAR_EVENTS];
  u32 chars_len;
  char text_input[MAX_TEXT_INPUT_LEN];
  u32 text_input_cursor_pos;
  b32 text_input_changed;
#ifdef DEBUG
  i32 _frame_update_and_end_stack;
#endif
} InputSystem;

/* initialize input system */
InputSystem input_init();
/* update input state from events */
void input_update(InputSystem *input, AppInputEvents *input_events, f32 now);
/* update single button state */
void input_update_button(AppButton *btn, AppInputEvent event);
/* update touch state */
void input_update_touch(MobileTouches *touches, AppInputEvent event, f32 time);
/* clear this_frame flags (call at end of frame) */
void input_end_frame(InputSystem *inputs);
#endif
