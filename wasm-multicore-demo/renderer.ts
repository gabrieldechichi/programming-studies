// WebGPU Renderer - separate from thread management

export interface Renderer {
    device: GPUDevice;
    context: GPUCanvasContext;
    format: GPUTextureFormat;
    clearColor: GPUColor;
}

export async function createRenderer(canvas: OffscreenCanvas): Promise<Renderer> {
    if (!navigator.gpu) {
        throw new Error("WebGPU not supported");
    }

    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
        throw new Error("No WebGPU adapter found");
    }

    const device = await adapter.requestDevice();

    const context = canvas.getContext("webgpu");
    if (!context) {
        throw new Error("Failed to get WebGPU context");
    }

    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({
        device,
        format,
        alphaMode: "opaque",
    });

    return {
        device,
        context,
        format,
        clearColor: { r: 0.1, g: 0.2, b: 0.3, a: 1.0 },
    };
}

export function renderFrame(renderer: Renderer): void {
    const { device, context, clearColor } = renderer;

    const encoder = device.createCommandEncoder();

    const textureView = context.getCurrentTexture().createView();

    const renderPass = encoder.beginRenderPass({
        colorAttachments: [
            {
                view: textureView,
                clearValue: clearColor,
                loadOp: "clear",
                storeOp: "store",
            },
        ],
    });

    renderPass.end();

    device.queue.submit([encoder.finish()]);
}
