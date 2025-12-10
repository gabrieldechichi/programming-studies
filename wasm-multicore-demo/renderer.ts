// WebGPU Renderer - separate from thread management

export interface Renderer {
    device: GPUDevice;
    context: GPUCanvasContext;
    format: GPUTextureFormat;
    canvas: OffscreenCanvas;
    depthTexture: GPUTexture;
}

// GPU resource arrays (indexed by handle idx)
const buffers: (GPUBuffer | null)[] = [];
const shaders: (GPUShaderModule | null)[] = [];
const pipelines: (GPURenderPipeline | null)[] = [];
const pipelineBindGroupLayouts: (GPUBindGroupLayout | null)[] = [];

// Current frame state
let currentEncoder: GPUCommandEncoder | null = null;
let currentPass: GPURenderPassEncoder | null = null;
let currentPipelineIdx: number = -1;
let currentBindings: {
    vertexBuffers: (GPUBuffer | null)[];
    indexBuffer: GPUBuffer | null;
    indexFormat: GPUIndexFormat;
    uniformBuffer: GPUBuffer | null;
    bindGroup: GPUBindGroup | null;
} = {
    vertexBuffers: [],
    indexBuffer: null,
    indexFormat: "uint16",
    uniformBuffer: null,
    bindGroup: null,
};

let renderer: Renderer | null = null;

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

    // Create depth texture
    const depthTexture = device.createTexture({
        size: [canvas.width, canvas.height],
        format: "depth24plus",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    renderer = {
        device,
        context,
        format,
        canvas,
        depthTexture,
    };

    return renderer;
}

// Helper to read string from WASM memory
function readString(memory: WebAssembly.Memory, ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

// Vertex format mapping
const VERTEX_FORMATS: GPUVertexFormat[] = [
    "float32x2", // GPU_VERTEX_FORMAT_FLOAT2
    "float32x3", // GPU_VERTEX_FORMAT_FLOAT3
    "float32x4", // GPU_VERTEX_FORMAT_FLOAT4
];

const INDEX_FORMATS: GPUIndexFormat[] = [
    "uint16", // GPU_INDEX_FORMAT_U16
    "uint32", // GPU_INDEX_FORMAT_U32
];

const PRIMITIVE_TOPOLOGIES: GPUPrimitiveTopology[] = [
    "triangle-list", // GPU_PRIMITIVE_TRIANGLES
    "line-list",     // GPU_PRIMITIVE_LINES
];

// Create GPU imports for WASM
export function createGpuImports(memory: WebAssembly.Memory) {
    return {
        js_gpu_init: () => {
            console.log("[GPU] Initialized");
        },

        js_gpu_make_buffer: (type: number, size: number, dataPtr: number): number => {
            if (!renderer) return -1;

            const usage = type === 0 ? GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST
                        : type === 1 ? GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST
                        : GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST;

            const buffer = renderer.device.createBuffer({
                size,
                usage,
                mappedAtCreation: dataPtr !== 0,
            });

            if (dataPtr !== 0) {
                const src = new Uint8Array(memory.buffer, dataPtr, size);
                const dst = new Uint8Array(buffer.getMappedRange());
                dst.set(src);
                buffer.unmap();
            }

            const idx = buffers.length;
            buffers.push(buffer);
            return idx;
        },

        js_gpu_update_buffer: (handleIdx: number, dataPtr: number, size: number) => {
            if (!renderer) return;
            const buffer = buffers[handleIdx];
            if (!buffer) return;

            const src = new Uint8Array(memory.buffer, dataPtr, size);
            renderer.device.queue.writeBuffer(buffer, 0, src);
        },

        js_gpu_destroy_buffer: (handleIdx: number) => {
            const buffer = buffers[handleIdx];
            if (buffer) {
                buffer.destroy();
                buffers[handleIdx] = null;
            }
        },

        js_gpu_make_shader: (vsPtr: number, vsLen: number, fsPtr: number, fsLen: number): number => {
            if (!renderer) return -1;

            const vsCode = readString(memory, vsPtr, vsLen);
            const fsCode = readString(memory, fsPtr, fsLen);

            // Combine into single module with @vertex and @fragment entry points
            const combinedCode = vsCode + "\n" + fsCode;

            const module = renderer.device.createShaderModule({
                code: combinedCode,
            });

            const idx = shaders.length;
            shaders.push(module);
            return idx;
        },

        js_gpu_destroy_shader: (handleIdx: number) => {
            // Shader modules don't need explicit destruction in WebGPU
            shaders[handleIdx] = null;
        },

        js_gpu_make_pipeline: (
            shaderIdx: number,
            layoutPtr: number,
            primitive: number,
            depthTest: number,
            depthWrite: number
        ): number => {
            if (!renderer) return -1;

            const shaderModule = shaders[shaderIdx];
            if (!shaderModule) return -1;

            // Read vertex layout from memory
            const layoutData = new Uint32Array(memory.buffer, layoutPtr, 2 + 8 * 3);
            const stride = layoutData[0];
            const attrCount = layoutData[1];

            const attributes: GPUVertexAttribute[] = [];
            for (let i = 0; i < attrCount; i++) {
                const format = layoutData[2 + i * 3 + 0];
                const offset = layoutData[2 + i * 3 + 1];
                const location = layoutData[2 + i * 3 + 2];
                attributes.push({
                    format: VERTEX_FORMATS[format],
                    offset,
                    shaderLocation: location,
                });
            }

            // Create bind group layout for uniforms
            const bindGroupLayout = renderer.device.createBindGroupLayout({
                entries: [
                    {
                        binding: 0,
                        visibility: GPUShaderStage.VERTEX,
                        buffer: { type: "uniform" },
                    },
                ],
            });

            const pipelineLayout = renderer.device.createPipelineLayout({
                bindGroupLayouts: [bindGroupLayout],
            });

            const pipeline = renderer.device.createRenderPipeline({
                layout: pipelineLayout,
                vertex: {
                    module: shaderModule,
                    entryPoint: "vs_main",
                    buffers: [
                        {
                            arrayStride: stride,
                            attributes,
                        },
                    ],
                },
                fragment: {
                    module: shaderModule,
                    entryPoint: "fs_main",
                    targets: [{ format: renderer.format }],
                },
                primitive: {
                    topology: PRIMITIVE_TOPOLOGIES[primitive],
                    cullMode: "back",
                },
                depthStencil: depthTest
                    ? {
                          format: "depth24plus",
                          depthWriteEnabled: !!depthWrite,
                          depthCompare: "less",
                      }
                    : undefined,
            });

            const idx = pipelines.length;
            pipelines.push(pipeline);
            pipelineBindGroupLayouts.push(bindGroupLayout);
            return idx;
        },

        js_gpu_destroy_pipeline: (handleIdx: number) => {
            pipelines[handleIdx] = null;
            pipelineBindGroupLayouts[handleIdx] = null;
        },

        js_gpu_begin_pass: (r: number, g: number, b: number, a: number, depth: number) => {
            if (!renderer) return;

            currentEncoder = renderer.device.createCommandEncoder();

            const textureView = renderer.context.getCurrentTexture().createView();

            currentPass = currentEncoder.beginRenderPass({
                colorAttachments: [
                    {
                        view: textureView,
                        clearValue: { r, g, b, a },
                        loadOp: "clear",
                        storeOp: "store",
                    },
                ],
                depthStencilAttachment: {
                    view: renderer.depthTexture.createView(),
                    depthClearValue: depth,
                    depthLoadOp: "clear",
                    depthStoreOp: "store",
                },
            });
        },

        js_gpu_apply_pipeline: (handleIdx: number) => {
            if (!currentPass) return;
            const pipeline = pipelines[handleIdx];
            if (!pipeline) return;

            currentPass.setPipeline(pipeline);
            currentPipelineIdx = handleIdx;
        },

        js_gpu_apply_bindings: (bindingsPtr: number) => {
            if (!currentPass || !renderer) return;

            // Read bindings from memory
            // Format: [vb_count, vb0_idx, vb1_idx, vb2_idx, vb3_idx, ib_idx, ib_format, ub_idx]
            const data = new Uint32Array(memory.buffer, bindingsPtr, 4 + 4);
            const vbCount = data[0];

            // Set vertex buffers
            for (let i = 0; i < vbCount; i++) {
                const bufIdx = data[1 + i];
                const buffer = buffers[bufIdx];
                if (buffer) {
                    currentPass.setVertexBuffer(i, buffer);
                }
            }

            // Set index buffer
            const ibIdx = data[5];
            const ibFormat = data[6];
            const indexBuffer = buffers[ibIdx];
            if (indexBuffer) {
                currentPass.setIndexBuffer(indexBuffer, INDEX_FORMATS[ibFormat]);
            }

            // Create and set bind group for uniform buffer
            const ubIdx = data[7];
            const uniformBuffer = buffers[ubIdx];
            if (uniformBuffer && currentPipelineIdx >= 0) {
                const bindGroupLayout = pipelineBindGroupLayouts[currentPipelineIdx];
                if (bindGroupLayout) {
                    const bindGroup = renderer.device.createBindGroup({
                        layout: bindGroupLayout,
                        entries: [
                            {
                                binding: 0,
                                resource: { buffer: uniformBuffer },
                            },
                        ],
                    });
                    currentPass.setBindGroup(0, bindGroup);
                }
            }
        },

        js_gpu_draw: (vertexCount: number, instanceCount: number) => {
            if (!currentPass) return;
            currentPass.draw(vertexCount, instanceCount);
        },

        js_gpu_draw_indexed: (indexCount: number, instanceCount: number) => {
            if (!currentPass) return;
            currentPass.drawIndexed(indexCount, instanceCount);
        },

        js_gpu_end_pass: () => {
            if (!currentPass) return;
            currentPass.end();
            currentPass = null;
        },

        js_gpu_commit: () => {
            if (!currentEncoder || !renderer) return;
            renderer.device.queue.submit([currentEncoder.finish()]);
            currentEncoder = null;
            currentPipelineIdx = -1;
        },
    };
}

// Legacy function for simple clear (can be removed later)
export function renderFrame(r: Renderer): void {
    const { device, context } = r;

    const encoder = device.createCommandEncoder();
    const textureView = context.getCurrentTexture().createView();

    const renderPass = encoder.beginRenderPass({
        colorAttachments: [
            {
                view: textureView,
                clearValue: { r: 0.1, g: 0.2, b: 0.3, a: 1.0 },
                loadOp: "clear",
                storeOp: "store",
            },
        ],
    });

    renderPass.end();
    device.queue.submit([encoder.finish()]);
}
