#include "camera.h"
#include "cglm/quat.h"
#include "cglm/vec3.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "vendor/cglm/util.h"

void camera_update(Camera *cam, GameInput *input, f32 dt) {
    UNUSED(dt);
  // Orbit camera controls - hold right mouse button to orbit
  if (input->mouse_right.is_pressed) {
    f32 orbit_sensitivity = 0.4f;

    // Update pitch and yaw based on mouse delta
    cam->pitch -= input->mouse_delta[1] * orbit_sensitivity;
    cam->yaw -= input->mouse_delta[0] * orbit_sensitivity;
    // Clamp pitch between -70 and 70 degrees
    cam->pitch = glm_clamp(cam->pitch, -85.0f, 85.0f);
  }

  // Set camera rotation from pitch and yaw
  quat_from_euler((vec3){glm_rad(cam->pitch), glm_rad(cam->yaw), 0}, cam->rot);

  // Zoom with scroll wheel
  if (input->scroll_delta[1] != 0.0f) {
    f32 zoom_sensitivity = 0.2f;
    cam->arm -= input->scroll_delta[1] * zoom_sensitivity;
    cam->arm = glm_clamp(cam->arm, 0.7f, 10.0f); // Min 0.5, max 50 units
  }

  // Pan camera - hold middle mouse button to pan
  if (input->mouse_middle.is_pressed) {
    f32 pan_sensitivity = (cam->arm / 3) * 0.35f / 60;

    vec3 pan_dir = VEC3(-input->mouse_delta[0] * pan_sensitivity,
                        input->mouse_delta[1] * pan_sensitivity, 0.0);
    glm_vec3_norm(pan_dir);
    glm_quat_rotatev(cam->rot, pan_dir, pan_dir);
    glm_vec3_add(cam->orbit_center, pan_dir, cam->orbit_center);
  }

  // Calculate camera position by moving back from orbit center along camera's
  // forward direction
  vec3 forward_dir;
  glm_quat_rotatev(cam->rot, (vec3){0, 0, -1},
                   forward_dir); // Camera looks down -Z

  // Move camera back by arm distance from orbit center
  glm_vec3_scale(forward_dir, -cam->arm,
                 forward_dir); // Negative to move backwards
  glm_vec3_add(cam->orbit_center, forward_dir, cam->pos);

  extract_frustum_planes(cam, &cam->frustum);
}

// todo: only update uniforms if did move
void camera_update_uniforms(Camera *camera, f32 canvas_width,
                            f32 canvas_height) {
  // quat_look_at((vec3){0, 0, 0}, game_state->camera.pos,
  //              game_state->camera.rot);
  quat_from_euler((vec3){glm_rad(camera->pitch), glm_rad(camera->yaw), 0},
                  camera->rot);
  vec3 look_dir;
  glm_quat_rotatev(camera->rot, (vec3){0, 0, -1}, look_dir);
  glm_look(camera->pos, look_dir, (vec3){0, 1, 0}, camera->uniforms.view_matrix);

  f32 aspect = canvas_width / canvas_height;
  f32 fov = camera->fov > 0 ? camera->fov : 60;

  // perspective matrix
  glm_perspective(glm_rad(fov), aspect, 0.1, 10000,
                  camera->uniforms.projection_matrix);

  // mvp matrix
  glm_mat4_mul(camera->uniforms.projection_matrix, camera->uniforms.view_matrix,
               camera->uniforms.view_proj_matrix);

  glm_mat4_inv_fast(camera->uniforms.view_proj_matrix,
                    camera->uniforms.inv_view_proj_matrix);

  glm_vec4((vec3){camera->pos[0], camera->pos[1], camera->pos[2]}, 0.0f,
           camera->uniforms.camera_pos);
  renderer_update_camera(&camera->uniforms);
}

void extract_frustum_planes(const Camera *camera, Frustum *frustum) {
  mat4 vp_matrix;
  memcpy(vp_matrix, camera->uniforms.view_proj_matrix, sizeof(mat4));
  glm_frustum_planes(vp_matrix, (vec4 *)frustum->planes);
}

b32 sphere_in_frustum(Frustum *frustum, vec3 center, f32 radius) {
  for (u32 i = 0; i < 6; i++) {
    Plane plane = frustum->planes[i];

    f32 distance = glm_vec3_dot(plane.normal, center) + plane.distance;

    if (distance < -radius) {
      return false;
    }
  }
  return true;
}

i32 update_lods(const LODLevel_Array lod_levels, vec3 camera_pos,
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
