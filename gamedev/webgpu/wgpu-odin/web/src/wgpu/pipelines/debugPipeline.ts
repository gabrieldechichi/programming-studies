import shaderSource from "resources/shaders/debug.wgsl?raw";

export type DebugPipelineCreateParams = {
  device: GPUDevice;
  textureFormat: GPUTextureFormat;
};

export class DebugPipelineGlobalUniforms {
  viewProjectionMatrix!: Float32Array;
  colors!: Float32Array;

  static viewProjectionMatrixOffset: number = 0;
  static viewProjectionMatrixFloatCount = 16;
  static viewProjectionMatrixByteSize = 16 * Float32Array.BYTES_PER_ELEMENT;

  static colorCount: number = 4;
  static colorsOffset: number = this.viewProjectionMatrixByteSize;
  static colorsFloatCount = 4 * this.colorCount;
  static colorsByteSize =
    this.colorsFloatCount * Float32Array.BYTES_PER_ELEMENT;

  static byteSize: number =
    this.viewProjectionMatrixByteSize + this.colorsByteSize;
}

export class DebugPipelineModelMatrixUniforms {
  static instanceCount = 1024;

  static modelMatricesOffset: number = 0;
  static modelMatricesFloatCount = 16 * this.instanceCount;
  static modelMatricesByteSize =
    this.modelMatricesFloatCount * Float32Array.BYTES_PER_ELEMENT;
  static elementByteSize = 16;

  static byteSize: number = this.modelMatricesByteSize;
}

export class DebugPipeline {
  pipeline!: GPURenderPipeline;
  globalUniformsGroupLayout!: GPUBindGroupLayout;
  instanceUniformsGroupLayout!: GPUBindGroupLayout;

  static create({ device, textureFormat }: DebugPipelineCreateParams) {
    const pipeline = new DebugPipeline();

    pipeline.globalUniformsGroupLayout = device.createBindGroupLayout({
      entries: [
        //global uniforms
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    pipeline.instanceUniformsGroupLayout = device.createBindGroupLayout({
      entries: [
        //model matrices
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
        bindGroupLayouts: [
          pipeline.globalUniformsGroupLayout,
          pipeline.instanceUniformsGroupLayout,
        ],
      }),
    });
    return pipeline;
  }
}
