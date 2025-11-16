/*
    math.h - Math utilities and wrappers

    OVERVIEW

    --- Wraps cglm library with convenience functions and macros

    --- vector math: vec3, vec4, quaternion operations

    --- matrix math: mat4 transformations

    --- utils: lerp, clamp, angle conversion, common constants

    USAGE
        vec3 pos = VEC3(1, 2, 3);
        quaternion rot;
        quat_from_euler(VEC3(RAD(90), 0, 0), rot);

        mat4 transform;
        mat_trs(pos, rot, VEC3_ONE, transform);

        f32 t = lerp(0.0f, 1.0f, 0.5f);
*/

#ifndef H_MATH
#define H_MATH

#include "array.h"
#include "cglm/cglm.h"
#include "common.h"
#include "typedefs.h"
#include <math.h>

force_inline bool32 fequal(f32 a, f32 b) { return fabs(a - b) < EPSILON; }

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define clamp(v, a, b) (min(max(v, a), b))
#define clamp01(v) clamp(v, 0, 1)

/*!
 * Begin: Linear algebra
 */
typedef versor quaternion;
arr_define(mat4);

#define abs(a) fabs(a)

#define approximately(a, b) (abs((a) - (b)) < EPSILON)

#define RADIANS(degrees) glm_rad(degrees)
#define RAD(degrees) RADIANS(degrees)
#define DEGREES(radians) glm_deg(radians)
#define DEG(radians) DEGREES(radians)

#define SQR(x) (x) * (x)
#define MAT4_IDENTITY GLM_MAT4_IDENTITY

#define FMT_VEC3(v) FMT_FLOAT(v[0]), FMT_FLOAT(v[1]), FMT_FLOAT(v[2])

#define VEC3(x, y, z) ((vec3){x, y, z})

#define VEC3_ZERO ((vec3){0})
#define VEC3_ONE ((vec3){1, 1, 1})
#define VEC3_RIGHT ((vec3){1.0f, 0.0f, 0.0f})
#define VEC3_UP ((vec3){0.0f, 1.0f, 0.0f})
#define VEC3_FORWARD ((vec3){0.0f, 0.0f, 1.0f})

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
  glm_vec3_lerp(a, b, t, dest);
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

force_inline void quat_look_at_dir_with_up(vec3 dir, vec3 up,
                                           _out_ quaternion rot) {
  // Note: glm_quat_for normalizes the direction
  glm_normalize(dir);
  glm_quat_for(dir, up, rot);
}

force_inline void quat_look_at_dir(vec3 dir, _out_ quaternion rot) {
  quat_look_at_dir_with_up(dir, (vec3){0, 1, 0}, rot);
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
  // todo: simd?
  translation[0] = mat[3][0];
  translation[1] = mat[3][1];
  translation[2] = mat[3][2];
}

force_inline void mat_get_rotation_euler(mat4 mat, _out_ vec3 euler_angles) {
  glm_euler_angles(mat, euler_angles);
}

force_inline void mat_get_rotation(mat4 mat, _out_ quaternion rot) {
  // decouple scale and rotation
  vec3 scale;
  mat4 rot_matrix;
  glm_decompose_rs(mat, rot_matrix, scale);

  glm_mat4_quat(rot_matrix, rot);
}

force_inline void mat_get_scale(mat4 mat, _out_ vec3 scale) {
  // decouple scale and rotation
  mat4 rot_matrix;
  glm_decompose_rs(mat, rot_matrix, scale);
}

/*!
 * @brief creates a matrix from a single rotation
 *
 * @param[in]   q     quaternion
 * @param[out]  mat   result matrix
 */
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

force_inline void mat_r(quaternion rotation, _out_ mat4 mat) {
  glm_quat_mat4(rotation, mat);
}

force_inline void mat_trs_euler(vec3 translation, vec3 rotation, vec3 scale,
                                _out_ mat4 mat) {
  quaternion quat;
  quat_from_euler(rotation, quat);
  mat_trs(translation, quat, scale, mat);
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

force_inline f32 smooth_damp(f32 current, f32 target, f32 *current_velocity,
                             f32 smooth_time, f32 max_speed, f32 delta_time) {
  smooth_time = fmaxf(0.0001f, smooth_time);
  f32 num = 2.0f / smooth_time;
  f32 num2 = num * delta_time;
  f32 num3 =
      1.0f / (1.0f + num2 + 0.48f * num2 * num2 + 0.235f * num2 * num2 * num2);
  f32 num4 = current - target;
  f32 num5 = target;
  f32 num6 = max_speed * smooth_time;

  num4 = clamp(num4, -num6, num6);
  target = current - num4;

  f32 num7 = (*current_velocity + num * num4) * delta_time;
  *current_velocity = (*current_velocity - num * num7) * num3;
  f32 num8 = target + (num4 + num7) * num3;

  if ((num5 - current > 0.0f) == (num8 > num5)) {
    num8 = num5;
    *current_velocity = (num8 - num5) / delta_time;
  }

  return num8;
}

force_inline b32 sphere_in_frustum(Frustum *frustum, vec3 center, f32 radius) {
  for (u32 i = 0; i < 6; i++) {
    Plane plane = frustum->planes[i];

    f32 distance = glm_vec3_dot(plane.normal, center) + plane.distance;

    if (distance < -radius) {
      return false;
    }
  }
  return true;
}

force_inline f32 arr_sum_f32(f32* arr, u32 len){
    f32 sum = 0;
    for (u32 i = 0; i < len; i++){
        sum += arr[i];
    }
    return sum;
}

force_inline u32 arr_sum_u32(u32* arr, u32 len){
    u32 sum = 0;
    for (u32 i = 0; i < len; i++){
        sum += arr[i];
    }
    return sum;
}

force_inline i32 arr_sum_i32(i32* arr, u32 len){
    i32 sum = 0;
    for (u32 i = 0; i < len; i++){
        sum += arr[i];
    }
    return sum;
}

force_inline i32 find_index_f32(f32* arr, u32 len, f32 v){
    for (u32 i = 0; i < len; i++){
        if (fequal(arr[i], v)){
            return i;
        }
    }
    return ARR_INVALID_INDEX;
}

#endif
