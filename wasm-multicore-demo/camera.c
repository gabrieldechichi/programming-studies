#include "camera.h"
#include "lib/math.h"
#include "assert.h"
#include "cglm/quat.h"
#include "cglm/vec3.h"
#include "lib/typedefs.h"
#include "cglm/util.h"

Camera camera_init(vec3 pos, vec3 rot, f32 fov) {
  Camera cam = {.fov = fov};
  vec3_copy(pos, cam.pos);
  quat_from_euler(rot, cam.rot);
  return cam;
}

void camera_update(Camera *camera, f32 canvas_width, f32 canvas_height) {
  camera_update_uniforms(camera, canvas_width, canvas_height);
  camera_extract_frustum_planes(camera, &camera->frustum);
}

void camera_update_uniforms(Camera *camera, f32 canvas_width, f32 canvas_height) {
  vec3 look_dir;
  glm_quat_rotatev(camera->rot, (vec3){0, 0, -1}, look_dir);
  glm_look(camera->pos, look_dir, (vec3){0, 1, 0}, camera->view);

  f32 aspect = canvas_width / canvas_height;
  f32 fov = camera->fov > 0 ? camera->fov : 60;

  glm_perspective(glm_rad(fov), aspect, 0.3f, 1000.0f, camera->proj);

  glm_mat4_mul(camera->proj, camera->view, camera->view_proj);
}

void camera_extract_frustum_planes(const Camera *camera, Frustum *frustum) {
  mat4 vp_matrix;
  memcpy(vp_matrix, camera->view_proj, sizeof(mat4));
  glm_frustum_planes(vp_matrix, (vec4 *)frustum->planes);
}

i32 camera_update_lods(const LODLevel_Array lod_levels, vec3 camera_pos,
                       vec3 entity_pos) {

  // calculate squared distance to camera for LOD selection
  vec3 cam_to_entity;
  glm_vec3_sub(entity_pos, camera_pos, cam_to_entity);
  f32 dist_squared = vec3_sqrlen(cam_to_entity);

  // determine LOD level based on distance using LOD config
  u32 lod_level = lod_levels.len - 1; // default to lowest quality
  for (u32 lod_idx = 0; lod_idx < lod_levels.len - 1; lod_idx++) {
    if (dist_squared < lod_levels.items[lod_idx].max_distance_squared) {
      lod_level = lod_idx;
      break;
    }
  }

  return lod_level;
}
