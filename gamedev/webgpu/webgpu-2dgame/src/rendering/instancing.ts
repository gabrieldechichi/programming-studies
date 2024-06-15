import {
  GPUIndexBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";

export class InstanceData {
  data!: Float32Array;
  instancesBuffer!: GPUVertexBuffer;
  geometryBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;
  floatStride!: number;
  indexCount!: number;
  capacity!: number;
  count!: number;

  static create(
    device: GPUDevice,
    capacity: number,
    floatStride: number,
    geometry: Float32Array,
    indices: Int16Array,
  ) {
    const instanceData = new InstanceData();
    instanceData.floatStride = floatStride;
    instanceData.data = new Float32Array(capacity * floatStride);
    instanceData.capacity = capacity;
    instanceData.count = 0;
    instanceData.instancesBuffer = createVertexBuffer(
      device,
      instanceData.data,
    );

    //prettier-ignore
    instanceData.geometryBuffer = createVertexBuffer(
      device,
      geometry
    );

    instanceData.indexBuffer = createIndexBuffer(device, indices);

    instanceData.indexCount = indices.length;
    return instanceData
  }
}
