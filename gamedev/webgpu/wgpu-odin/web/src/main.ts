import { DebugRenderer } from "./wgpu/renderers/debugRenderer";
import { DiscordSDK } from "@discord/embedded-app-sdk";

const queryParams = new URLSearchParams(window.location.search);
const isEmbedded = queryParams.has("frame_id");
let discordSdk: DiscordSDK | undefined;

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

function proxyPath(s: string): string {
  if (!s || s.length === 0) {
    return "";
  }
  if (s[0] === '.' || s[0] === '/'){
      s = s.substring(1)
  }
  if (isEmbedded) {
    return `.proxy/${s}`;
  }
  return `./${s}`;
}

async function main() {
  if (isEmbedded) {
    console.log("wait for discord", import.meta.env.VITE_DISCORD_CLIENT_ID);
    discordSdk = new DiscordSDK(import.meta.env.VITE_DISCORD_CLIENT_ID);
    await discordSdk.ready();
    console.log("finish discord");
  }

  const renderer = await Renderer.initialize();
  if (!renderer) {
    return;
  }
  const memoryInterface = new window.odin.WasmMemoryInterface();
  await window.odin.runWasm(
    proxyPath("/resources/game.wasm"),
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
        wgpu_debugRendererAddBatch: (
          uniformsPtr: number,
          instanceCount: number,
          instanceFloatCount: number,
        ) => {
          const uniforms = memoryInterface.loadF32Array(
            uniformsPtr,
            instanceCount * instanceFloatCount,
          );
          renderer.debugRenderer.addInstanceBatch(
            renderer.device,
            uniforms,
            instanceCount,
          );
        },
        wgpu_debugRendererSetBatches: (
          uniformsPtr: number,
          instanceCount: number,
          instanceFloatCount: number,
        ) => {
          const uniforms = memoryInterface.loadF32Array(
            uniformsPtr,
            instanceCount * instanceFloatCount,
          );

          renderer.debugRenderer.setAllBatches(
            renderer.device,
            uniforms,
            instanceCount,
            instanceFloatCount,
          );
        },
      },
    },
    memoryInterface,
  );
}

main().then(() => console.log("done"));
