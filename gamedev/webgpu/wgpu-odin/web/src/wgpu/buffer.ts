interface Branding<BrandName> {
  _type: BrandName;
}

export type GPUVertexBuffer = GPUBuffer & Branding<"GPUVertexBuffer">;
export type GPUIndexBuffer = GPUBuffer & Branding<"GPUIndexBuffer">;
export type GPUUniformBuffer = GPUBuffer & Branding<"GPUUniformBuffer">;

export const MAT4_BYTE_LENGTH: number = 16 * Float32Array.BYTES_PER_ELEMENT;

export class WGPUBuffer {
  static createVertexBuffer(
    device: GPUDevice,
    data: Float32Array,
  ): GPUVertexBuffer {
    const buffer = device.createBuffer({
      size: data.byteLength,
      usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
      mappedAtCreation: true,
    });

    new Float32Array(buffer.getMappedRange()).set(data);
    buffer.unmap();
    return buffer as GPUVertexBuffer;
  }

  static createIndexBuffer(
    device: GPUDevice,
    data: Int16Array,
  ): GPUIndexBuffer {
    const buffer = device.createBuffer({
      size: data.byteLength,
      usage: GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST,
      mappedAtCreation: true,
    });

    new Int16Array(buffer.getMappedRange()).set(data);
    buffer.unmap();
    return buffer as GPUIndexBuffer;
  }

  static createUniformBuffer(
    device: GPUDevice,
    byteLength: number,
  ): GPUUniformBuffer {
    const buffer = device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    return buffer as GPUUniformBuffer;
  }
}
