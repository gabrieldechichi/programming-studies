#include "flycam.h"
#include "camera.h"
#include "cglm/types.h"
#include "input.h"
#include "lib/math.h"
#include "lib/typedefs.h"

void flycam_update(FlyCameraCtrl *camera_ctrl, Camera *camera,
                   InputSystem *input, f32 dt) {
  local_persist b32 is_locked = false;
  if (input->buttons[MOUSE_LEFT].pressed_this_frame) {
    is_locked = true;
    // os_lock_mouse(true);
  } else if (input->buttons[MOUSE_LEFT].released_this_frame) {
    is_locked = false;
    // os_lock_mouse(false);
  }

  f32 move_speed = camera_ctrl->move_speed;
  f32 mouse_sensitivity = 0.002f;

  if (is_locked) {
    camera_ctrl->camera_yaw -= input->mouse_delta[0] * mouse_sensitivity;
    camera_ctrl->camera_pitch -= input->mouse_delta[1] * mouse_sensitivity;

    f32 max_pitch = RAD(89);
    if (camera_ctrl->camera_pitch > max_pitch)
      camera_ctrl->camera_pitch = max_pitch;
    if (camera_ctrl->camera_pitch < -max_pitch)
      camera_ctrl->camera_pitch = -max_pitch;
  }

  quaternion quat_yaw, quat_pitch;
  glm_quatv(quat_yaw, camera_ctrl->camera_yaw, VEC3_UP);
  vec3 right_axis;
  glm_quat_rotatev(quat_yaw, VEC3_RIGHT, right_axis);
  glm_quatv(quat_pitch, camera_ctrl->camera_pitch, right_axis);
  glm_quat_mul(quat_pitch, quat_yaw, camera->rot);

  if (input->buttons[KEY_W].is_pressed) {
    vec3 forward;
    camera_forward(camera, forward);
    glm_vec3_scale(forward, -move_speed * dt, forward);
    glm_vec3_add(camera_ctrl->camera_pos, forward, camera_ctrl->camera_pos);
  }
  if (input->buttons[KEY_S].is_pressed) {
    vec3 forward;
    camera_forward(camera, forward);
    glm_vec3_scale(forward, move_speed * dt, forward);
    glm_vec3_add(camera_ctrl->camera_pos, forward, camera_ctrl->camera_pos);
  }
  if (input->buttons[KEY_A].is_pressed) {
    vec3 right;
    camera_right(camera, right);
    glm_vec3_scale(right, -move_speed * dt, right);
    glm_vec3_add(camera_ctrl->camera_pos, right, camera_ctrl->camera_pos);
  }
  if (input->buttons[KEY_D].is_pressed) {
    vec3 right;
    camera_right(camera, right);
    glm_vec3_scale(right, move_speed * dt, right);
    glm_vec3_add(camera_ctrl->camera_pos, right, camera_ctrl->camera_pos);
  }

  glm_vec3_copy(camera_ctrl->camera_pos, camera->pos);
}

void flycam_update_camera_transform(FlyCameraCtrl *camera_ctrl, Camera *camera) {
  quaternion quat_yaw, quat_pitch;
  glm_quatv(quat_yaw, camera_ctrl->camera_yaw, VEC3_UP);
  vec3 right_axis;
  glm_quat_rotatev(quat_yaw, VEC3_RIGHT, right_axis);
  glm_quatv(quat_pitch, camera_ctrl->camera_pitch, right_axis);
  glm_quat_mul(quat_pitch, quat_yaw, camera->rot);
  glm_vec3_copy(camera_ctrl->camera_pos, camera->pos);
}
