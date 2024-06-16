import { Texture } from "../texture";
import {
  GPUIndexBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import shaderSource from "../shader/post-process.wgsl?raw";

export class PostProcessPipeline {
  texture!: Texture;
  textureBindGroup!: GPUBindGroup;
  pipeline!: GPURenderPipeline;
  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;

  static async create(device: GPUDevice, width: number, height: number) {
    const pipeline = new PostProcessPipeline();

    pipeline.texture = await Texture.createEmptyTexture(device, width, height);

    //prettier-ignore
    pipeline.vertexBuffer = createVertexBuffer(device, new Float32Array([
        //pos        //uv
        -1.0, -1.0,    0.0, 1.0,
        1.0, -1.0,    1.0, 1.0,
        1.0, 1.0,    1.0, 0.0,
        -1.0, 1.0,    0.0, 0.0,
    ]))
    //prettier-ignore
    pipeline.indexBuffer = createIndexBuffer(device, new Int16Array([
        0,1,2,
        2,3,0
    ]))

    const textureGroupLayout = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          sampler: {},
        },
        {
          binding: 1,
          visibility: GPUShaderStage.FRAGMENT,
          texture: {},
        },
      ],
    });

    pipeline.textureBindGroup = device.createBindGroup({
      layout: textureGroupLayout,
      entries: [
        {
          binding: 0,
          resource: pipeline.texture.sampler,
        },
        {
          binding: 1,
          resource: pipeline.texture.texture.createView(),
        },
      ],
    });

    const module = device.createShaderModule({ code: shaderSource });

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [
        {
          arrayStride: (2 + 2) * Float32Array.BYTES_PER_ELEMENT,
          stepMode: "vertex",
          attributes: [
            //pos
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
      ],
    };

    const fragment: GPUFragmentState = {
      module,
      entryPoint: "fragmentMain",
      targets: [
        {
          format: navigator.gpu.getPreferredCanvasFormat(),
        },
      ],
    };

    pipeline.pipeline = device.createRenderPipeline({
      label: "post process",
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: device.createPipelineLayout({
        bindGroupLayouts: [textureGroupLayout],
      }),
    });
    return pipeline;
  }
}

export class PostProcessRenderer {
  device!: GPUDevice;
  pipeline!: PostProcessPipeline;
  static async create(
    device: GPUDevice,
    screenWidth: number,
    screenHeight: number,
  ) {
    const renderer = new PostProcessRenderer();
    renderer.device = device;
    renderer.pipeline = await PostProcessPipeline.create(
      device,
      screenWidth,
      screenHeight,
    );
    return renderer;
  }

  render(target: GPUTextureView) {
    const commandEncoder = this.device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass({
      label: "post process",
      colorAttachments: [{ view: target, loadOp: "load", storeOp: "store" }],
    });

    passEncoder.setPipeline(this.pipeline.pipeline);
    passEncoder.setVertexBuffer(0, this.pipeline.vertexBuffer);
    passEncoder.setIndexBuffer(this.pipeline.indexBuffer, "uint16");
    passEncoder.setBindGroup(0, this.pipeline.textureBindGroup);
    passEncoder.drawIndexed(6);
    passEncoder.end();
    this.device.queue.submit([commandEncoder.finish()]);
  }
}
