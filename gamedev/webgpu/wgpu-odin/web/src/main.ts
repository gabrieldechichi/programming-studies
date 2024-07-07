import { DebugRenderer } from "./wgpu/renderers/debugRenderer";

class Renderer {
  canvas!: HTMLCanvasElement;
  ctx!: GPUCanvasContext;
  adapter!: GPUAdapter;
  device!: GPUDevice;
  format!: GPUTextureFormat;

  debugRenderer!: DebugRenderer;

  static async initialize(): Promise<Renderer | null> {
    const canvas = document.getElementById("canvas") as HTMLCanvasElement;
    const ctx = canvas.getContext("webgpu");
    if (!ctx) {
      alert("WebGPU not supported");
      return null;
    }

    const adapter = await navigator.gpu.requestAdapter({
      powerPreference: "low-power",
    });

    if (!adapter) {
      alert("adapter not found");
      return null;
    }

    const device = await adapter.requestDevice();

    const format = navigator.gpu.getPreferredCanvasFormat();
    ctx.configure({ device, format });

    const debugRenderer = DebugRenderer.create({
      device,
      textureFormat: format,
    });

    const renderer = new Renderer();
    renderer.canvas = canvas;
    renderer.ctx = ctx;
    renderer.adapter = adapter;
    renderer.device = device;
    renderer.format = format;
    renderer.debugRenderer = debugRenderer;

    return renderer;
  }

  render({ projectionViewMatrix }: { projectionViewMatrix: Float32Array }) {
    const { ctx, device, debugRenderer } = this;
    const targetTexture = ctx.getCurrentTexture();
    const cmdEncoder = device.createCommandEncoder();
    const passEncoder = cmdEncoder.beginRenderPass({
      label: "render",
      colorAttachments: [
        {
          view: targetTexture.createView(),
          clearValue: [0.2, 0.2, 0.2, 1],
          loadOp: "clear",
          storeOp: "store",
        } as GPURenderPassColorAttachment,
      ],
    });

    const queue = device.queue;
    debugRenderer.render({ passEncoder, queue, projectionViewMatrix });
    passEncoder.end();
    queue.submit([cmdEncoder.finish()]);

    debugRenderer.endFrame();
  }
}

async function main() {
  const renderer = await Renderer.initialize();
  if (!renderer) {
    return;
  }
  const memoryInterface = new window.odin.WasmMemoryInterface();
  await window.odin.runWasm(
    "./resources/game.wasm",
    null,
    {
      env: {
        wgpu_render: (viewProjectionPtr: number) => {
          const viewProjectionArray = memoryInterface.loadF32Array(
            viewProjectionPtr,
            16,
          );
          renderer.render({ projectionViewMatrix: viewProjectionArray });
        },
        wgpu_debugRendererDrawSquare: (modelMatrixPtr: number) => {
          const modelMatrix = memoryInterface.loadF32Array(modelMatrixPtr, 16);
          renderer.debugRenderer.addInstance({ modelMatrix });
        },
      },
    },
    memoryInterface,
  );
}

main().then(() => console.log("done"));
