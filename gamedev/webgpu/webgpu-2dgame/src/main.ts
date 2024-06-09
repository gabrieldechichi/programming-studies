import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./bufferUtils";
import { Camera } from "./camera";
import { Content } from "./content";
import { Quad } from "./geometry";
import shaderSource from "./shader/shader.wgsl?raw";
import { Texture } from "./texture";

class Renderer {
  private context!: GPUCanvasContext;
  private device!: GPUDevice;
  private pipeline!: GPURenderPipeline;
  private camera!: Camera;
  private projectionViewBuffer!: GPUUniformBuffer;
  private player!: Quad;
  private projectionViewBindGroup!: GPUBindGroup;
  private textureBindGroup!: GPUBindGroup;

  public async initialize() {
    const canvas = document.getElementById("canvas") as HTMLCanvasElement;

    const ctx = canvas.getContext("webgpu");
    if (!ctx) {
      alert("WebGPU not supported!");
      return;
    }
    this.context = ctx;

    const adapter = await navigator.gpu.requestAdapter({
      powerPreference: "low-power",
    });

    if (!adapter) {
      alert("adapter not found");
      return;
    }

    this.device = await adapter.requestDevice();

    this.context.configure({
      device: this.device,
      format: navigator.gpu.getPreferredCanvasFormat(),
    });

    {
      this.camera = new Camera(800, 600);
      this.camera.update();

      this.projectionViewBuffer = createUniformBuffer(
        this.device,
        MAT4_BYTE_LENGTH,
      );
    }

    //model stuff
    {
      await Content.initialize(this.device);
      await this.preparePipeline();
      this.player = new Quad(this.device, [0, 0], [99, 75]);
    }
  }

  async preparePipeline() {
    const module = this.device.createShaderModule({ code: shaderSource });

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

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [vertexBufferLayout],
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
              srcFactor: "one",
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
    const textureGroupLayout = this.device.createBindGroupLayout({
      entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} },
      ],
    });

    const playerTexture = Content.playerTexture;

    this.textureBindGroup = this.device.createBindGroup({
      layout: textureGroupLayout,
      entries: [
        {
          binding: 0,
          resource: playerTexture.sampler,
        },
        {
          binding: 1,
          resource: playerTexture.texture.createView(),
        },
      ],
    });

    const projectionViewBufferLayout = this.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    this.projectionViewBindGroup = this.device.createBindGroup({
      layout: projectionViewBufferLayout,
      entries: [
        {
          binding: 0,
          resource: {
            buffer: this.projectionViewBuffer,
          },
        },
      ],
    });

    const pipelineLayout = this.device.createPipelineLayout({
      bindGroupLayouts: [projectionViewBufferLayout, textureGroupLayout],
    });

    this.pipeline = this.device.createRenderPipeline({
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: pipelineLayout,
    });
  }

  public render() {
    //update view
    {
      this.camera.update();
      this.device.queue.writeBuffer(
        this.projectionViewBuffer,
        0,
        this.camera.viewProjection as Float32Array,
      );
    }

    //render
    {
      const commandEncoder = this.device.createCommandEncoder();
      const textureViewer = this.context.getCurrentTexture().createView();
      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureViewer,
            clearValue: { r: 0.8, g: 0.8, b: 0.8, a: 1.0 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(this.pipeline);

      passEncoder.setIndexBuffer(this.player.indexBuffer, "uint16");
      passEncoder.setVertexBuffer(0, this.player.vertexBuffer);

      passEncoder.setBindGroup(0, this.projectionViewBindGroup);
      passEncoder.setBindGroup(1, this.textureBindGroup);
      passEncoder.drawIndexed(6);
      passEncoder.end();

      this.device.queue.submit([commandEncoder.finish()]);
    }
  }
}

async function main() {
  const renderer = new Renderer();
  await renderer.initialize();
  renderer.render();
}

main().then(() => console.log("done"));
