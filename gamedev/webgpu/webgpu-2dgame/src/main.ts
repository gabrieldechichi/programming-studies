import { Quad } from "./geometry";
import shaderSource from "./shader/shader.wgsl?raw";

class Renderer {
  private context!: GPUCanvasContext;
  private device!: GPUDevice;
  private pipeline!: GPURenderPipeline;
  private quad!: Quad;

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

    //model stuff
    {
      this.preparePipeline();
      this.quad = new Quad(this.device);
    }
  }

  preparePipeline() {
    const module = this.device.createShaderModule({ code: shaderSource });

    const posBuffer: GPUVertexBufferLayout = {
      arrayStride: 2 * Float32Array.BYTES_PER_ELEMENT,
      stepMode: "vertex",
      attributes: [
        {
          shaderLocation: 0,
          offset: 0,
          format: "float32x2",
        },
      ],
    };

    const colorBuffer: GPUVertexBufferLayout = {
      arrayStride: 4 * Float32Array.BYTES_PER_ELEMENT,
      stepMode: "vertex",
      attributes: [
        {
          shaderLocation: 1,
          offset: 0,
          format: "float32x4",
        },
      ],
    };

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [posBuffer, colorBuffer],
    };

    const fragment: GPUFragmentState = {
      module,
      entryPoint: "fragmentMain",
      targets: [{ format: navigator.gpu.getPreferredCanvasFormat() }],
    };

    this.pipeline = this.device.createRenderPipeline({
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: "auto",
    });
  }

  public render() {
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
    passEncoder.setIndexBuffer(this.quad.indexBuffer, "uint16");
    passEncoder.setVertexBuffer(0, this.quad.positionBuffer);
    passEncoder.setVertexBuffer(1, this.quad.colorBuffer);
    passEncoder.drawIndexed(6);
    passEncoder.end();

    this.device.queue.submit([commandEncoder.finish()]);
  }
}

async function main() {
  const renderer = new Renderer();
  await renderer.initialize();
  renderer.render();
}

main().then(() => console.log("done"));
