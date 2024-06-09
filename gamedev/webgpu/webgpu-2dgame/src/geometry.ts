import { GPUIndexBuffer, GPUVertexBuffer, createIndexBuffer, createVertexBuffer } from "./bufferUtils";

export class Quad {
  //prettier-ignore
  positions: number[] = [
      -0.5, -0.5,
      0.5, -0.5,
      0.5, 0.5,
      -0.5, 0.5
  ];

  //prettier-ignore
  colors:number[] = [
      1.0, 0.0, 0.0, 1.0,
      0.0, 1.0, 0.0, 1.0,
      0.0, 0.0, 1.0, 1.0,
      1.0, 1.0, 0.0, 1.0,
  ];

  //prettier-ignore
  indexes: number[] = [
      0, 1, 2,
      2, 3, 0
  ];

  positionBuffer!: GPUVertexBuffer;
  colorBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;

  constructor(device: GPUDevice) {
    this.positionBuffer = createVertexBuffer(
      device,
      new Float32Array(this.positions),
    );
    this.colorBuffer = createVertexBuffer(
      device,
      new Float32Array(this.colors),
    );
    this.indexBuffer = createIndexBuffer(device, new Int16Array(this.indexes));
  }
}
