type RendererData = {
  canvas: HTMLCanvasElement;
  ctx: GPUCanvasContext;
  adapter: GPUAdapter;
  device: GPUDevice;
  format: GPUTextureFormat;
};

class Renderer {
  static render({ ctx, device }: RendererData) {
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
    passEncoder.end();
    device.queue.submit([cmdEncoder.finish()]);
  }
}

async function main() {
  const canvas = document.getElementById("canvas") as HTMLCanvasElement;
  const ctx = canvas.getContext("webgpu");
  if (!ctx) {
    alert("WebGPU not supported");
    return;
  }

  const adapter = await navigator.gpu.requestAdapter({
    powerPreference: "low-power",
  });

  if (!adapter) {
    alert("adapter not found");
    return;
  }

  const device = await adapter.requestDevice();

  const format = navigator.gpu.getPreferredCanvasFormat();
  ctx.configure({ device, format });

  const renderer = {
    canvas,
    ctx,
    adapter,
    device,
    format,
  } as RendererData;
  Renderer.render(renderer);
}

main().then(() => console.log("done"));
