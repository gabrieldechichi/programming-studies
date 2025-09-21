#ifndef H_MATH
#define H_MATH

#include "array.h"
#include "cglm/cglm.h"
#include "common.h"
#include "typedefs.h"
#include <math.h>

bool32 fequal(f32 a, f32 b) { return fabs(a - b) < EPSILON; }

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define clamp(v, a, b) (min(max(v, a), b))
#define clamp01(v) clamp(v, 0, 1)

/*!
 * Begin: Linear algebra
 */
typedef versor quaternion;
arr_define(mat4);
slice_define(mat4);
#define abs(a) fabs(a)

#define approximately(a, b) (abs((a) - (b)) < EPSILON)

#define SQR(x) (x) * (x)
#define MAT4_IDENTITY GLM_MAT4_IDENTITY

#define FMT_VEC3(v) FMT_FLOAT(v[0]), FMT_FLOAT(v[1]), FMT_FLOAT(v[2])

#define VEC3(x, y, z) ((vec3){x, y, z})

force_inline f32 lerp_inverse(f32 a, f32 b, f32 current) {
  return glm_percent(a, b, current);
}
force_inline f32 lerp_inverse_clamped(f32 a, f32 b, f32 current) {
  return glm_percentc(a, b, current);
}

force_inline f32 lerp(f32 a, f32 b, f32 t) { return glm_lerp(a, b, t); }
force_inline f32 lerpc(f32 a, f32 b, f32 t) { return glm_lerpc(a, b, t); }

force_inline void vec3_copy(vec3 a, vec3 dest) { glm_vec3_copy(a, dest); }
force_inline void vec3_lerp(vec3 a, vec3 b, float t, vec3 dest) {
  return glm_vec3_lerp(a, b, t, dest);
}

force_inline f32 vec3_sqrlen(vec3 v) { return glm_vec3_norm2(v); }

force_inline void vec4_copy(vec4 a, vec4 dest) { glm_vec4_copy(a, dest); }

force_inline void quat_copy(quaternion q, quaternion dest) {
  glm_quat_copy(q, dest);
}

force_inline void quat_slerp(quaternion from, quaternion to, float t,
                             quaternion dest) {
  glm_quat_slerp(from, to, t, dest);
}

force_inline void quat_nlerp(quaternion from, quaternion to, float t,
                             quaternion dest) {
  glm_quat_nlerp(from, to, t, dest);
}

force_inline void quat_look_at_with_up(vec3 point, vec3 from, vec3 up,
                                       _out_ quaternion rot) {
  vec3 dir = {0};
  glm_vec3_sub(point, from, dir);
  // Note: glm_quat_for normalizes the direction
  glm_normalize(dir);
  glm_quat_for(dir, up, rot);
}

force_inline void quat_look_at(vec3 point, vec3 from, _out_ quaternion rot) {
  quat_look_at_with_up(point, from, cast(vec3){0, 1, 0}, rot);
}

force_inline void quat_from_euler_xyz(vec3 angles, quaternion dest) {
  glm_euler_xyz_quat(angles, dest);
}

force_inline void quat_from_euler(vec3 angles, quaternion dest) {
  glm_euler_yxz_quat(angles, dest);
}

force_inline void quat_identity(quaternion q) { glm_quat_identity(q); }

force_inline void quat_scale(quaternion q, f32 scale, quaternion out) {
  glm_vec4_scale(q, scale, out);
}

force_inline void quat_add_shortest_path(quaternion a, quaternion b,
                                         quaternion out) {
  // ensure shortest path by checking dot product
  f32 dot = glm_quat_dot(a, b);
  quaternion b_to_use;
  if (dot < 0.0f) {
    quat_scale(b, -1.0f, b_to_use);
  } else {
    glm_quat_copy(b, b_to_use);
  }

  glm_quat_add(a, b_to_use, out);
  glm_quat_normalize(out);
}

force_inline void mat4_identity(mat4 mat) { glm_mat4_identity(mat); }
force_inline void mat4_mul(mat4 m1, mat4 m2, mat4 dest) {

  glm_mat4_mul(m1, m2, dest);
}

force_inline void mat4_get_translation(mat4 mat, _out_ vec3 translation) {
  translation[0] = mat[3][0];
  translation[1] = mat[3][1];
  translation[2] = mat[3][2];
}

/*!
 * @brief creates a matrix from a single rotation
 *
 * @param[in]   q     quaternion
 * @param[out]  mat   result matrix
 */
force_inline void mat_r(quaternion rotation, _out_ mat4 mat) {
  glm_quat_mat4(rotation, mat);
}

force_inline void mat4_inv_fast(mat4 mat, mat4 dest) {
  glm_mat4_inv_fast(mat, dest);
}

force_inline void mat4_inv(mat4 mat, mat4 dest) { glm_mat4_inv(mat, dest); }

force_inline void mat_tr(vec3 translation, quaternion rotation,
                         _out_ mat4 mat) {
  glm_mat4_identity(mat);
  glm_translate(mat, translation);
  glm_quat_rotate(mat, rotation, mat);
}

force_inline void mat_trs(vec3 translation, quaternion rotation, vec3 scale,
                          _out_ mat4 mat) {
  glm_mat4_identity(mat);
  glm_translate(mat, translation);
  glm_quat_rotate(mat, rotation, mat);
  glm_scale(mat, scale);
}

force_inline void mat_t(vec3 translation, _out_ mat4 mat) {
  glm_mat4_identity(mat);
  glm_translate(mat, translation);
}

force_inline void mat_s(vec3 scale, _out_ mat4 mat) {
  glm_mat4_identity(mat);
  glm_scale(mat, scale);
}
/*!
 * End: Linear algebra
 */

typedef struct {
  union {
    struct {
      vec3 normal;
      f32 distance;
    };
    vec4 normal_and_dist;
  };
} Plane;

typedef struct {
  union {
    struct {
      Plane left;
      Plane right;
      Plane bottom;
      Plane top;
      Plane near;
      Plane far;
    };
    Plane planes[6];
  };
} Frustum;

#endif
