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

  static moveTowards(
    current: vec2,
    target: vec2,
    maxDistanceDelta: number,
  ): vec2 {
    const toTarget: vec2 = [target[0] - current[0], target[1] - current[1]];
    const magnitude = vec2.len(toTarget);
    if (magnitude <= maxDistanceDelta || magnitude == 0) {
      return target;
    }
    const dx = (toTarget[0] / magnitude) * maxDistanceDelta;
    const dy = (toTarget[1] / magnitude) * maxDistanceDelta;

    return [current[0] + dx, current[1] + dy] as vec2;
  }
}
