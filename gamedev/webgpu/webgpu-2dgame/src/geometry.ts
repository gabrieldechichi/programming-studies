import {
  GPUIndexBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";

export class Quad {
  //prettier-ignore
  vertices = [
      // xy        //uv         //color
      -0.5, -0.5,  0.0, 1.0,    1.0, 1.0, 1.0, 1.0,
      0.5, -0.5,   1.0, 1.0,    1.0, 1.0, 1.0, 1.0,
      0.5, 0.5,    1.0, 0.0,    1.0, 1.0, 1.0, 1.0,
      -0.5, 0.5,   0.0, 0.0,    1.0, 1.0, 1.0, 1.0,
  ];

  //prettier-ignore
  indexes: number[] = [
      0, 1, 2,
      2, 3, 0
  ];

  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;

  constructor(device: GPUDevice) {
    this.vertexBuffer = createVertexBuffer(
      device,
      new Float32Array(this.vertices),
    );
    this.indexBuffer = createIndexBuffer(device, new Int16Array(this.indexes));
  }
}
