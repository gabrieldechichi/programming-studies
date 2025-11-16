#ifndef H_CAMERA
#define H_CAMERA
#include "input.h"
#include "lib/math.h"
#include "renderer/renderer.h"

typedef struct {
  vec3 pos;
  quaternion rot;

  f32 pitch;
  f32 yaw;
  f32 fov;
  f32 arm;
  vec3 orbit_center;

  Frustum frustum;
  CameraUniformBlock uniforms;
} Camera;

typedef struct {
  // opaque handle, can point to any renderer
  Handle renderer_id;
  f32 max_distance_squared; // entities beyond this distance use next LOD
} LODLevel;
arr_define(LODLevel);

i32 update_lods(const LODLevel_Array lod_levels, vec3 camera_pos,
                vec3 entity_pos);

void camera_update(Camera *cam, GameInput *input, f32 dt);
void camera_update_uniforms(Camera *camera, f32 canvas_width,
                            f32 canvas_height);

void extract_frustum_planes(const Camera *camera, Frustum *frustum);

b32 sphere_in_frustum(Frustum *frustum, vec3 center, f32 radius);
#endif
