/*
    camera.h - 3D camera

    OVERVIEW

    --- Camera with position, rotation (quaternion), and FOV

    --- generates view/projection matrices for renderer

    --- LOD system based on distance to camera

    USAGE
        Camera cam = camera_init(VEC3(0, 0, 5), VEC3_ZERO, 60.0f);

        camera_update_uniforms(&cam, width, height);
        renderer_update_camera(&cam.uniforms);

        vec3 forward;
        camera_forward(&cam, forward);
*/

#ifndef H_CAMERA
#define H_CAMERA
#include "lib/math.h"
#include "lib/handle.h"

/* 3D camera with position, rotation, and projection */
typedef struct {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  Frustum frustum;
  quaternion rot;
  vec3 pos;
  f32 fov;
} Camera;

/* LOD level with distance threshold */
typedef struct {
  Handle renderer_id;
  f32 max_distance_squared;
} LODLevel;
arr_define(LODLevel);

/* create camera */
Camera camera_init(vec3 pos, vec3 rot, f32 fov);

void camera_update(Camera *camera, f32 canvas_width, f32 canvas_height);

/* get LOD level based on distance to camera */
i32 camera_update_lods(const LODLevel_Array lod_levels, vec3 camera_pos,
                       vec3 entity_pos);

/* update camera matrices (call before rendering) */
void camera_update_uniforms(Camera *camera, f32 canvas_width, f32 canvas_height);

/* extract frustum planes for culling */
void camera_extract_frustum_planes(const Camera *camera, Frustum *frustum);

force_inline void camera_forward(Camera *cam, _out_ vec3 dir) {
  glm_quat_rotatev(cam->rot, VEC3_FORWARD, dir);
};
force_inline void camera_right(Camera *cam, _out_ vec3 dir) {
  glm_quat_rotatev(cam->rot, VEC3_RIGHT, dir);
};
force_inline void camera_up(Camera *cam, _out_ vec3 dir) {
  glm_quat_rotatev(cam->rot, VEC3_UP, dir);
};
#endif
