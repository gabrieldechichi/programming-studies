import { runWasm } from "./runtime";
import { GPUUniformBuffer, MAT4_BYTE_LENGTH, WGPUBuffer } from "./wgpu/buffer";

type RendererData = {
  canvas: HTMLCanvasElement;
  ctx: GPUCanvasContext;
  adapter: GPUAdapter;
  device: GPUDevice;
  format: GPUTextureFormat;
  projectionViewBuffer: GPUUniformBuffer;
};

type FrameData = {
  projectionViewMatrix: Float32Array;
};

class Renderer {
  static async initialize(): Promise<RendererData | null> {
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

    const projectionViewBuffer = WGPUBuffer.createUniformBuffer(
      device,
      MAT4_BYTE_LENGTH,
    );

    const renderer = {
      canvas,
      ctx,
      adapter,
      device,
      format,
      projectionViewBuffer,
    } as RendererData;

    return renderer;
  }

  static render(
    { projectionViewMatrix }: FrameData,
    { ctx, device, projectionViewBuffer }: RendererData,
  ) {
    const targetTexture = ctx.getCurrentTexture();
    const cmdEncoder = device.createCommandEncoder();
    const passEncoder = cmdEncoder.beginRenderPass({
      label: "render",
      colorAttachments: [
        {
          view: targetTexture.createView(),
          clearValue: [1, 0, 0, 1],
          loadOp: "clear",
          storeOp: "store",
        } as GPURenderPassColorAttachment,
      ],
    });

    device.queue.writeBuffer(projectionViewBuffer, 0, projectionViewMatrix);
    passEncoder.end();
    device.queue.submit([cmdEncoder.finish()]);
  }
}

async function main() {
  const renderer = await Renderer.initialize();
  if (!renderer) {
    return;
  }
  const memoryInterface = new window.odin.WasmMemoryInterface();
  console.log(memoryInterface.exports);
  await runWasm(
    "./resources/game.wasm",
    undefined,
    {
      env: {
        //this is a pointer to a mat4x4 defined as an array
        wgpu_render: (viewProjectionPtr: number) => {
          const viewProjectionArray = memoryInterface.loadF32Array(
            viewProjectionPtr,
            16,
          );
          Renderer.render(
            { projectionViewMatrix: viewProjectionArray },
            renderer,
          );
        },
      },
    },
    memoryInterface,
  );
  console.log("end");
  console.log(memoryInterface);
}
main().then(() => console.log("done"));
