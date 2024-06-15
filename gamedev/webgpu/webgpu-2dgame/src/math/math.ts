import { mat4, quat, vec2 } from "gl-matrix";

export type Transform = {
  pos: vec2;
  rot: number;
  size: vec2;
};

export class MathUtils {
  static trs(transform: Transform) {
    return mat4.fromRotationTranslationScale(
      mat4.create(),
      quat.fromEuler(quat.create(), 0, 0, (180 * transform.rot) / Math.PI),
      [transform.pos[0], transform.pos[1], 0],
      [transform.size[0], transform.size[1], 0],
    );
  }

  static rotateVertex(v: vec2, origin: vec2, rot: number): vec2 {
    return vec2.rotate(vec2.create(), v, origin, rot);
  }
}
