import { GPUUniformBuffer, MAT4_BYTE_LENGTH } from "./bufferUtils";
import shaderSource from "./shader/shader.wgsl?raw";
import { Texture } from "./texture";

export class SpritePipeline {
  pipeline!: GPURenderPipeline;
  textureBindGroup!: GPUBindGroup;
  projectionViewBindGroup!: GPUBindGroup;

  static FLOATS_PER_VERTEX: number = 2 + 2 + 4; //xy + uv + color
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

    //vertex and fragment stuff
    const vertexBufferLayout: GPUVertexBufferLayout = {
      arrayStride: (2 + 2 + 4) * Float32Array.BYTES_PER_ELEMENT,
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
        //color
        {
          shaderLocation: 2,
          offset: (2 + 2) * Float32Array.BYTES_PER_ELEMENT,
          format: "float32x4",
        },
      ],
    };

    const modelBufferLayout: GPUVertexBufferLayout = {
      arrayStride: MAT4_BYTE_LENGTH,
      stepMode: "instance",
      attributes: [
        //row1
        {
          shaderLocation: 3,
          offset: 0,
          format: "float32x4",
        },
        //row2
        {
          shaderLocation: 4,
          offset: 1 * 4 * Float32Array.BYTES_PER_ELEMENT,
          format: "float32x4",
        },
        //row3
        {
          shaderLocation: 5,
          offset: 2 * 4 * Float32Array.BYTES_PER_ELEMENT,
          format: "float32x4",
        },
        //row4
        {
          shaderLocation: 6,
          offset: 3 * 4 * Float32Array.BYTES_PER_ELEMENT,
          format: "float32x4",
        },
      ],
    };

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [vertexBufferLayout, modelBufferLayout],
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
              srcFactor: "src-alpha",
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

    const pipelineLayout = device.createPipelineLayout({
      bindGroupLayouts: [projectionViewBufferLayout, textureGroupLayout],
    });

    this.pipeline = device.createRenderPipeline({
      label: texture.id,
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: pipelineLayout,
    });
  }
}
