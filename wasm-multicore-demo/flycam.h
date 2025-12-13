#ifndef FLYCAM_H
#define FLYCAM_H

#include "camera.h"
#include "cglm/types.h"
#include "input.h"
#include "lib/math.h"
#include "lib/typedefs.h"

typedef struct {
  vec3 camera_pos;
  f32 camera_yaw;
  f32 camera_pitch;

  f32 move_speed;
} FlyCameraCtrl;

void flycam_update(FlyCameraCtrl *camera_ctrl, Camera* camera, InputSystem *input, f32 dt);

void flycam_update_camera_transform(FlyCameraCtrl *camera_ctrl, Camera *camera);

#endif 
