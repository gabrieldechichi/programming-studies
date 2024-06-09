interface Branding<BrandName> {
  _type: BrandName;
}

export type GPUVertexBuffer = GPUBuffer & Branding<"GPUVertexBuffer">;
export type GPUIndexBuffer = GPUBuffer & Branding<"GPUIndexBuffer">;

export function createVertexBuffer(
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

export function createIndexBuffer(
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
