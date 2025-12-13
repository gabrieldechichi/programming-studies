#include "input.h"
#include "lib/array.h"
#include "lib/assert.h"
#include "os/os.h"
#include "cglm/vec2.h"

void input_update_button(AppButton *btn, AppInputEvent event) {
  if (event.type == INPUT_EVENT_KEYUP) {
    btn->released_this_frame = btn->is_pressed;
  } else if (event.type == INPUT_EVENT_KEYDOWN) {
    btn->pressed_this_frame = !btn->is_pressed;
    btn->is_pressed = true;
  }
}

void input_update_touch(MobileTouches *touches, AppInputEvent event, f32 time) {
  switch (event.type) {
  case INPUT_EVENT_TOUCH_START: {
    u32 touch_idx = event.touch.id;
    if (touch_idx > MAX_TOUCHES) {
      LOG_INFO(
          "Too many concurrent touches, max %. Skipping new touch with id %",
          FMT_UINT(touches->len), FMT_UINT(event.touch.id));
      return;
    }
    MobileTouch *touch = &touches->items[event.touch.id];
    touch->id = event.touch.id;
    touch->start_time = time;
    touch->is_active = true;
    touch->started_this_frame = true;
    touch->stopped_this_frame = false;
    touch->start_x = event.touch.x;
    touch->start_y = event.touch.y;
    touch->current_x = event.touch.x;
    touch->current_y = event.touch.y;
    touch->prev_frame_x = event.touch.x;
    touch->prev_frame_y = event.touch.y;

    touches->len++;
    break;
  }
  case INPUT_EVENT_TOUCH_END: {
    i32 touch_idx = event.touch.id;
    debug_assert_msg(
        touch_idx != ARR_INVALID_INDEX,
        "Received touch end event for touch with ID %, but touch is "
        "not on the active touches list",
        FMT_UINT(event.touch.id));
    if (touch_idx == ARR_INVALID_INDEX) {
      return;
    }
    MobileTouch *touch = &touches->items[touch_idx];

    touch->is_active = false;
    touch->started_this_frame = false;
    touch->stopped_this_frame = true;

    touch->prev_frame_x = touch->current_x;
    touch->prev_frame_y = touch->current_y;

    touch->current_x = event.touch.x;
    touch->current_y = event.touch.y;
    break;
  }
  case INPUT_EVENT_TOUCH_MOVE: {
    i32 touch_idx = event.touch.id;
    debug_assert_msg(touch_idx != ARR_INVALID_INDEX,
                     "Received touch move event for touch with ID %, but touch "
                     "is not on the active touches list",
                     FMT_UINT(event.touch.id));
    if (touch_idx == ARR_INVALID_INDEX) {
      return;
    }
    MobileTouch *touch = &touches->items[touch_idx];
    touch->is_active = true;
    touch->started_this_frame = false;
    touch->stopped_this_frame = false;
    touch->prev_frame_x = touch->current_x;
    touch->prev_frame_y = touch->current_y;

    touch->current_x = event.touch.x;
    touch->current_y = event.touch.y;
    break;
  }
  case INPUT_EVENT_KEYDOWN:
  case INPUT_EVENT_KEYUP:
  case INPUT_EVENT_CHAR:
  case INPUT_EVENT_TEXT_INPUT:
  case INPUT_EVENT_MAX:
    debug_assert_msg(false, "Called update_touch with wrong event type %",
                     FMT_STR(input_event_names[event.type]));
    break;
  case INPUT_EVENT_SCROLL:
    break;
  }
}

InputSystem input_init() {
  InputSystem input = {0};
  // input.touches.len = MAX_TOUCHES;
  static_assert(ARRAY_SIZE(input.touches.items) == MAX_TOUCHES,
                "input.touches should match capacity of MAX_TOUCHES");
  return input;
}

void input_update(InputSystem *input, AppInputEvents *input_events, f32 now) {

#ifdef DEBUG
  debug_assert_msg(input->_frame_update_and_end_stack == 0,
                   "input_update called twice without calling input_end_frame. "
                   "input_end_frame should be called after update, at the end "
                   "of the frame or the input processing scope");
  input->_frame_update_and_end_stack++;
#endif

  for (u32 i = 0; i < input_events->len; i++) {
    AppInputEvent e = input_events->events[i];

    switch (e.type) {

    case INPUT_EVENT_KEYDOWN:
    case INPUT_EVENT_KEYUP: {
      if (e.key.type >= 0 && e.key.type < (i32)ARRAY_SIZE(input->buttons)) {
        input_update_button(&input->buttons[e.key.type], e);
      }
      break;
    }
    case INPUT_EVENT_TOUCH_START:
    case INPUT_EVENT_TOUCH_END:
    case INPUT_EVENT_TOUCH_MOVE:
      input_update_touch(&input->touches, e, now);
      break;
    case INPUT_EVENT_SCROLL:
      input->scroll_delta[0] = e.scroll.delta_x;
      input->scroll_delta[1] = e.scroll.delta_y;
      break;
    case INPUT_EVENT_CHAR:
      if (input->chars_len < MAX_CHAR_EVENTS) {
        input->chars[input->chars_len++] = e.character.codepoint;
      } else {
        LOG_WARN(
            "Character buffer overflow, max %. Skipping char with codepoint %",
            FMT_UINT(MAX_CHAR_EVENTS), FMT_UINT(e.character.codepoint));
      }
      break;
    case INPUT_EVENT_TEXT_INPUT:
      if (e.text_input.text) {
        u32 len = 0;
        while (e.text_input.text[len] && len < MAX_TEXT_INPUT_LEN - 1) {
          input->text_input[len] = e.text_input.text[len];
          len++;
        }
        input->text_input[len] = '\0';
      } else {
        input->text_input[0] = '\0';
      }
      input->text_input_cursor_pos = e.text_input.cursor_pos;
      input->text_input_changed = true;
      break;
    case INPUT_EVENT_MAX:
      break;
    }
  }

  vec2 prev_mouse_pos;
  glm_vec2(input->mouse_pos, prev_mouse_pos);
  input->mouse_pos[0] = input_events->mouse_x;
  input->mouse_pos[1] = input_events->mouse_y;
  glm_vec2_sub(input->mouse_pos, prev_mouse_pos, input->mouse_delta);
}

void input_end_frame(InputSystem *inputs) {
#ifdef DEBUG
  debug_assert_msg(inputs->_frame_update_and_end_stack == 1,
                   "input_end_frame called without calling input_update first");
  inputs->_frame_update_and_end_stack--;
#endif
  for (u32 i = 0; i < ARRAY_SIZE(inputs->buttons); i++) {
    if (inputs->buttons[i].released_this_frame) {
      inputs->buttons[i].is_pressed = false;
    }
    inputs->buttons[i].released_this_frame = false;
    inputs->buttons[i].pressed_this_frame = false;
  }

  inputs->scroll_delta[0] = 0.0f;
  inputs->scroll_delta[1] = 0.0f;

  inputs->chars_len = 0;
  inputs->text_input_changed = false;

  for (i32 i = inputs->touches.len - 1; i >= 0; i--) {
    MobileTouch *touch = &inputs->touches.items[i];
    touch->started_this_frame = false;
    touch->prev_frame_x = touch->current_x;
    touch->prev_frame_y = touch->current_y;

    if (touch->stopped_this_frame) {
      touch->is_active = false;
      inputs->touches.len--;
    }
  }
}
