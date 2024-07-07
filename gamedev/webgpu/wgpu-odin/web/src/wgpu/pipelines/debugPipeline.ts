import shaderSource from "resources/shaders/debug.wgsl?raw";

export type DebugPipelineCreateParams = {
  device: GPUDevice;
  textureFormat: GPUTextureFormat;
};

export class DebugPipelineUniforms {
  viewProjectionMatrix!: Float32Array;
  //array
  modelMatrices!: Float32Array;

  static instanceCount = 256;

  static viewProjectionMatrixOffset: number = 0;
  static viewProjectionMatrixFloatCount = 16;
  static viewProjectionMatrixByteSize = 16 * Float32Array.BYTES_PER_ELEMENT;

  static modelMatricesOffset: number = this.viewProjectionMatrixByteSize;
  static modelMatricesFloatCount = 16 * this.instanceCount;
  static modelMatricesByteSize =
    this.modelMatricesFloatCount * Float32Array.BYTES_PER_ELEMENT;

  static byteSize: number =
    this.modelMatricesByteSize + this.viewProjectionMatrixByteSize;
}

export class DebugPipeline {
  pipeline!: GPURenderPipeline;
  uniformsGroupLayout!: GPUBindGroupLayout;

  static create({ device, textureFormat }: DebugPipelineCreateParams) {
    const pipeline = new DebugPipeline();

    pipeline.uniformsGroupLayout = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    const module = device.createShaderModule({ code: shaderSource });

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [
        {
          arrayStride: 2 * Float32Array.BYTES_PER_ELEMENT,
          stepMode: "vertex",
          attributes: [
            //vertex Pos
            {
              shaderLocation: 0,
              offset: 0,
              format: "float32x2",
            },
          ],
        },
      ],
    };

    const fragment: GPUFragmentState = {
      module,
      entryPoint: "fragmentMain",
      targets: [
        {
          format: textureFormat,
          blend: {
            color: {
              operation: "add",
              srcFactor: "src-alpha",
              dstFactor: "one-minus-src-alpha",
            },
            alpha: {
              operation: "add",
              srcFactor: "one",
              dstFactor: "one-minus-src-alpha",
            },
          },
        },
      ],
    };

    pipeline.pipeline = device.createRenderPipeline({
      label: "debug",
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: device.createPipelineLayout({
        bindGroupLayouts: [pipeline.uniformsGroupLayout],
      }),
    });
    return pipeline;
  }
}
