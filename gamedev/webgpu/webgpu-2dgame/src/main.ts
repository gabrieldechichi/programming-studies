import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./bufferUtils";
import { Camera } from "./camera";
import { Content } from "./content";
import { SpriteRenderer } from "./spriteRenderer";

class Renderer {
  private canvas!: HTMLCanvasElement;
  private context!: GPUCanvasContext;
  private device!: GPUDevice;
  private camera!: Camera;
  private projectionViewBuffer!: GPUUniformBuffer;
  private spriteRenderer!: SpriteRenderer;

  public async initialize() {
    const canvas = document.getElementById("canvas") as HTMLCanvasElement;
    this.canvas = canvas;
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

      this.projectionViewBuffer = createUniformBuffer(
        this.device,
        MAT4_BYTE_LENGTH,
      );
    }

    this.spriteRenderer = SpriteRenderer.create(
      this.device,
      this.projectionViewBuffer,
    );

    await Content.initialize(this.device);
  }

  public render() {
    //update view
    {
      this.camera.update();
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

      this.spriteRenderer.startFrame(this.camera.viewProjection);
      for (let i = 0; i < 100; i++) {
        this.spriteRenderer.render(Content.playerTexture, [
          (Math.random() - 0.5) * this.canvas.width,
          (Math.random() - 0.5) * this.canvas.height,
        ]);
      }
      this.spriteRenderer.endFrame(passEncoder);

      passEncoder.end();

      this.device.queue.submit([commandEncoder.finish()]);

      window.requestAnimationFrame(() => this.render())
    }
  }
}

async function main() {
  const renderer = new Renderer();
  await renderer.initialize();
  renderer.render();
}

main().then(() => console.log("done"));
