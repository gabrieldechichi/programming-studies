import { mat4, vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./bufferUtils";
import { Camera } from "./camera";
import { Content } from "./content";
import { Quad } from "./geometry";
import shaderSource from "./shader/shader.wgsl?raw";

class Renderer {
  private context!: GPUCanvasContext;
  private device!: GPUDevice;
  private pipeline!: GPURenderPipeline;
  private camera!: Camera;
  private mvpBuffer!: GPUUniformBuffer;
  private player!: Quad;
  private playerPos!: vec2;
  private mvpBindGroup!: GPUBindGroup;
  private textureBindGroup!: GPUBindGroup;

  public async initialize() {
    const canvas = document.getElementById("canvas") as HTMLCanvasElement;
    const width = canvas.width;
    const height = canvas.height;

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
      this.camera = new Camera(width, height);
      this.camera.update();

      this.mvpBuffer = createUniformBuffer(this.device, MAT4_BYTE_LENGTH);
    }

    //model stuff
    {
      await Content.initialize(this.device);
      await this.preparePipeline();
      this.player = new Quad(this.device, [0, 0], [99, 75]);
      this.playerPos = [-100, -100];
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

    const mvpBufferLayout = this.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    this.mvpBindGroup = this.device.createBindGroup({
      layout: mvpBufferLayout,
      entries: [
        {
          binding: 0,
          resource: {
            buffer: this.mvpBuffer,
          },
        },
      ],
    });

    const pipelineLayout = this.device.createPipelineLayout({
      bindGroupLayouts: [mvpBufferLayout, textureGroupLayout],
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
      const playerTrs = mat4.fromTranslation(mat4.create(), [
        this.playerPos[0],
        this.playerPos[1],
        0,
      ]);
      const playerMvp = mat4.multiply(mat4.create(), this.camera.viewProjection, playerTrs);
      this.camera.update();
      this.device.queue.writeBuffer(
        this.mvpBuffer,
        0,
        playerMvp as Float32Array,
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

      passEncoder.setBindGroup(0, this.mvpBindGroup);
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
