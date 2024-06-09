import { mat4 } from "gl-matrix";

export class Camera {
  view!: mat4;
  projection!: mat4;
  viewProjection!: mat4;

  width: number;
  height: number;

  constructor(width: number, height: number) {
    this.viewProjection = mat4.create();
    this.width = width;
    this.height = height;
  }

  update() {
    this.projection = mat4.ortho(
      mat4.create(),
      -this.width / 2,
      this.width / 2,
      -this.height / 2,
      this.height / 2,
      -1,
      1,
    );

    this.view = mat4.lookAt(mat4.create(), [0, 0, 1], [0, 0, 0], [0, 1, 0]);

    mat4.multiply(this.viewProjection, this.projection, this.view);
  }
}
