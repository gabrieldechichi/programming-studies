import { vec2 } from "gl-matrix";
import {
  GPUIndexBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";

export class Quad {
  //prettier-ignore
  vertices : number[] = [
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

  constructor(device: GPUDevice, _pos: vec2, size: vec2) {
    const e = vec2.scale(vec2.create(), size, 0.5);
    //prettier-ignore
    this.vertices = [
      // xy           //uv         //color
      -e[0], -e[1],   0.0, 1.0,    1.0, 1.0, 1.0, 1.0,
       e[0], -e[1],   1.0, 1.0,    1.0, 1.0, 1.0, 1.0,
       e[0],  e[1],   1.0, 0.0,    1.0, 1.0, 1.0, 1.0,
      -e[0],  e[1],   0.0, 0.0,    1.0, 1.0, 1.0, 1.0,
    ];
    this.vertexBuffer = createVertexBuffer(
      device,
      new Float32Array(this.vertices),
    );
    this.indexBuffer = createIndexBuffer(device, new Int16Array(this.indexes));
  }
}
