import { GPUUniformBuffer } from "./rendering/bufferUtils";
import shaderSource from "./shader/shader.wgsl?raw";
import { Texture } from "./texture";

export class SpritePipeline {
  pipeline!: GPURenderPipeline;
  textureBindGroup!: GPUBindGroup;
  projectionViewBindGroup!: GPUBindGroup;

  static VERTEX_INSTANCE_FLOAT_NUM = 16 + 2 + 2; //mat4x4 + uvOffset + uvScale
  static FLOATS_PER_VERTEX: number = 2 + 2; //xy + uv
  static VERTEX_STRIDE: number =
    SpritePipeline.FLOATS_PER_VERTEX * Float32Array.BYTES_PER_ELEMENT;

  static create(
    device: GPUDevice,
    texture: Texture,
    projectionViewBuffer: GPUUniformBuffer,
  ): SpritePipeline {
    const pipeline = new SpritePipeline();
    pipeline.initialize(device, texture, projectionViewBuffer);
    return pipeline;
  }

  initialize(
    device: GPUDevice,
    texture: Texture,
    projectionViewBuffer: GPUUniformBuffer,
  ) {
    const module = device.createShaderModule({ code: shaderSource });

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [
        {
          arrayStride: SpritePipeline.VERTEX_STRIDE,
          stepMode: "vertex",
          attributes: [
            //position
            {
              shaderLocation: 0,
              offset: 0,
              format: "float32x2",
            },
            //uv
            {
              shaderLocation: 1,
              offset: 2 * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x2",
            },
          ],
        },
        {
          arrayStride:
            SpritePipeline.VERTEX_INSTANCE_FLOAT_NUM *
            Float32Array.BYTES_PER_ELEMENT,
          stepMode: "instance",
          attributes: [
            //model matrix line 1
            {
              shaderLocation: 2,
              offset: 0,
              format: "float32x4",
            },
            //model matrix line 2
            {
              shaderLocation: 3,
              offset: 4 * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //model matrix line 3
            {
              shaderLocation: 4,
              offset: (4 + 4) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //model matrix line 4
            {
              shaderLocation: 5,
              offset: (4 + 4 + 4) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //uv offset
            {
              shaderLocation: 6,
              offset: (4 + 4 + 4 + 4) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x2",
            },
            //uv scale
            {
              shaderLocation: 7,
              offset: (4 + 4 + 4 + 4 + 2) * Float32Array.BYTES_PER_ELEMENT,
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
          format: navigator.gpu.getPreferredCanvasFormat(),
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

    //texture binding
    const textureGroupLayout = device.createBindGroupLayout({
      entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} },
      ],
    });

    this.textureBindGroup = device.createBindGroup({
      layout: textureGroupLayout,
      entries: [
        {
          binding: 0,
          resource: texture.sampler,
        },
        {
          binding: 1,
          resource: texture.texture.createView(),
        },
      ],
    });

    const projectionViewBufferLayout = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    this.projectionViewBindGroup = device.createBindGroup({
      layout: projectionViewBufferLayout,
      entries: [
        {
          binding: 0,
          resource: {
            buffer: projectionViewBuffer,
          },
        },
      ],
    });

    this.pipeline = device.createRenderPipeline({
      label: texture.id,
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: device.createPipelineLayout({
        bindGroupLayouts: [projectionViewBufferLayout, textureGroupLayout],
      }),
    });
  }
}
