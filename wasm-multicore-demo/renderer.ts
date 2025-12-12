// WebGPU Renderer - separate from thread management

import { readString } from "./shared.ts";

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

// Per-pipeline bind group layouts: [0] = uniforms (group 0), [1] = storage (group 1)
const pipelineBindGroupLayouts: [GPUBindGroupLayout | null, GPUBindGroupLayout | null][] = [];

// Per-pipeline uniform block info (sizes needed for bind group creation)
const pipelineUbInfo: { count: number; sizes: number[] }[] = [];

// Per-pipeline storage buffer info (count needed for bind group creation)
const pipelineSbInfo: { count: number }[] = [];

// Per-pipeline bind groups for dynamic uniforms (created once, reused with different offsets)
const pipelineUniformBindGroups: Map<string, GPUBindGroup> = new Map();

// Current frame state
let currentEncoder: GPUCommandEncoder | null = null;
let currentPass: GPURenderPassEncoder | null = null;
let currentPipelineIdx: number = -1;

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

// Map GpuShaderStage enum to WebGPU shader stage flags
function stageToGPU(stage: number): GPUShaderStageFlags {
    // GPU_STAGE_NONE = 0, GPU_STAGE_VERTEX = 1, GPU_STAGE_FRAGMENT = 2, GPU_STAGE_VERTEX_FRAGMENT = 3
    switch (stage) {
        case 1: return GPUShaderStage.VERTEX;
        case 2: return GPUShaderStage.FRAGMENT;
        case 3: return GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT;
        default: return GPUShaderStage.VERTEX; // fallback
    }
}

// Create GPU imports for WASM
export function createGpuImports(memory: WebAssembly.Memory) {
    return {
        js_gpu_init: () => {
            console.log("[GPU] Initialized");
        },

        js_gpu_make_buffer: (idx: number, type: number, size: number, dataPtr: number): void => {
            if (!renderer) return;

            // type: 0=vertex, 1=index, 2=uniform, 3=storage
            const usage = type === 0 ? GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST
                        : type === 1 ? GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST
                        : type === 2 ? GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
                        : GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST;

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

            buffers[idx] = buffer;
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

        js_gpu_make_shader: (idx: number, vsPtr: number, vsLen: number, fsPtr: number, fsLen: number): void => {
            if (!renderer) return;

            //todo: use WasmMemory
            //todo: separate readString multi-threaded vs single threaded
            const vsCode = readString(memory, vsPtr, vsLen);
            const fsCode = readString(memory, fsPtr, fsLen);

            // Combine into single module with @vertex and @fragment entry points
            const combinedCode = vsCode + "\n" + fsCode;

            const module = renderer.device.createShaderModule({
                code: combinedCode,
            });

            shaders[idx] = module;
        },

        js_gpu_destroy_shader: (handleIdx: number) => {
            // Shader modules don't need explicit destruction in WebGPU
            shaders[handleIdx] = null;
        },

        js_gpu_make_pipeline: (
            idx: number,
            shaderIdx: number,
            stride: number,
            attrCount: number,
            attrFormatsPtr: number,
            attrOffsetsPtr: number,
            attrLocationsPtr: number,
            ubCount: number,
            ubStagesPtr: number,
            ubSizesPtr: number,
            ubBindingsPtr: number,
            sbCount: number,
            sbStagesPtr: number,
            sbBindingsPtr: number,
            sbReadonlyPtr: number,
            primitive: number,
            depthTest: number,
            depthWrite: number
        ): void => {
            if (!renderer) return;

            const shaderModule = shaders[shaderIdx];
            if (!shaderModule) return;

            // Read vertex attribute arrays from memory
            const attrFormats = new Uint32Array(memory.buffer, attrFormatsPtr, attrCount);
            const attrOffsets = new Uint32Array(memory.buffer, attrOffsetsPtr, attrCount);
            const attrLocations = new Uint32Array(memory.buffer, attrLocationsPtr, attrCount);

            const attributes: GPUVertexAttribute[] = [];
            for (let i = 0; i < attrCount; i++) {
                attributes.push({
                    format: VERTEX_FORMATS[attrFormats[i]],
                    offset: attrOffsets[i],
                    shaderLocation: attrLocations[i],
                });
            }

            // Read uniform block arrays from memory
            const ubStages = new Uint32Array(memory.buffer, ubStagesPtr, ubCount);
            const ubSizes = new Uint32Array(memory.buffer, ubSizesPtr, ubCount);
            const ubBindings = new Uint32Array(memory.buffer, ubBindingsPtr, ubCount);

            // Store UB info for bind group creation later
            const sizes: number[] = [];
            for (let i = 0; i < ubCount; i++) {
                sizes.push(ubSizes[i]);
            }
            pipelineUbInfo[idx] = { count: ubCount, sizes };

            // Read storage buffer arrays from memory
            const sbStages = new Uint32Array(memory.buffer, sbStagesPtr, sbCount);
            const sbBindings = new Uint32Array(memory.buffer, sbBindingsPtr, sbCount);
            const sbReadonly = new Uint32Array(memory.buffer, sbReadonlyPtr, sbCount);

            // Store SB info for bind group creation later
            pipelineSbInfo[idx] = { count: sbCount };

            // Create bind group layout for uniforms (group 0)
            const uniformLayoutEntries: GPUBindGroupLayoutEntry[] = [];
            for (let i = 0; i < ubCount; i++) {
                uniformLayoutEntries.push({
                    binding: ubBindings[i],
                    visibility: stageToGPU(ubStages[i]),
                    buffer: {
                        type: "uniform",
                        hasDynamicOffset: true,
                    },
                });
            }

            const uniformBindGroupLayout = ubCount > 0
                ? renderer.device.createBindGroupLayout({ entries: uniformLayoutEntries })
                : null;

            // Create bind group layout for storage buffers (group 1)
            const storageLayoutEntries: GPUBindGroupLayoutEntry[] = [];
            for (let i = 0; i < sbCount; i++) {
                storageLayoutEntries.push({
                    binding: sbBindings[i],
                    visibility: stageToGPU(sbStages[i]),
                    buffer: {
                        type: sbReadonly[i] ? "read-only-storage" : "storage",
                    },
                });
            }

            const storageBindGroupLayout = sbCount > 0
                ? renderer.device.createBindGroupLayout({ entries: storageLayoutEntries })
                : null;

            // Build pipeline layout with available bind group layouts
            const bindGroupLayouts: GPUBindGroupLayout[] = [];
            if (uniformBindGroupLayout) bindGroupLayouts.push(uniformBindGroupLayout);
            if (storageBindGroupLayout) {
                // If we have storage but no uniforms, we need a placeholder for group 0
                if (!uniformBindGroupLayout) {
                    const emptyLayout = renderer.device.createBindGroupLayout({ entries: [] });
                    bindGroupLayouts.push(emptyLayout);
                }
                bindGroupLayouts.push(storageBindGroupLayout);
            }

            const pipelineLayout = renderer.device.createPipelineLayout({
                bindGroupLayouts: bindGroupLayouts.length > 0 ? bindGroupLayouts : undefined,
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

            pipelines[idx] = pipeline;
            pipelineBindGroupLayouts[idx] = [uniformBindGroupLayout, storageBindGroupLayout];
        },

        js_gpu_destroy_pipeline: (handleIdx: number) => {
            pipelines[handleIdx] = null;
            pipelineBindGroupLayouts[handleIdx] = [null, null];
            delete pipelineUbInfo[handleIdx];
            delete pipelineSbInfo[handleIdx];
        },

        js_gpu_begin_pass: (r: number, g: number, b: number, a: number, depth: number) => {
            if (!renderer) return;

            //todo: cache command encoder??
            currentEncoder = renderer.device.createCommandEncoder();

            //todo: cache view
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
            //todo: error if currentPass not valid
            if (!currentPass) return;
            const pipeline = pipelines[handleIdx];
            if (!pipeline) return;

            currentPass.setPipeline(pipeline);
            currentPipelineIdx = handleIdx;
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
            //todo: can I use the same encoder for all cmds and commit at the end?
            if (!currentEncoder || !renderer) return;
            renderer.device.queue.submit([currentEncoder.finish()]);
            currentEncoder = null;
            currentPipelineIdx = -1;
        },

        js_gpu_upload_uniforms: (bufIdx: number, dataPtr: number, size: number) => {
            if (!renderer) return;
            const buffer = buffers[bufIdx];
            if (!buffer) return;

            const src = new Uint8Array(memory.buffer, dataPtr, size);
            renderer.device.queue.writeBuffer(buffer, 0, src);
        },

        js_gpu_apply_bindings: (
            vbCount: number,
            vbIndicesPtr: number,
            ibIdx: number,
            ibFormat: number,
            uniformBufIdx: number,
            ubCount: number,
            ubOffsetsPtr: number,
            sbCount: number,
            sbIndicesPtr: number
        ) => {
            if (!currentPass || !renderer) return;

            // Read vertex buffer indices from memory
            const vbIndices = new Uint32Array(memory.buffer, vbIndicesPtr, vbCount);

            // Set vertex buffers
            for (let i = 0; i < vbCount; i++) {
                const buffer = buffers[vbIndices[i]];
                if (buffer) {
                    currentPass.setVertexBuffer(i, buffer);
                }
            }

            // Set index buffer
            const indexBuffer = buffers[ibIdx];
            if (indexBuffer) {
                currentPass.setIndexBuffer(indexBuffer, INDEX_FORMATS[ibFormat]);
            }

            // Get or create bind groups for this pipeline
            if (currentPipelineIdx >= 0) {
                const layouts = pipelineBindGroupLayouts[currentPipelineIdx];
                const uniformBuffer = buffers[uniformBufIdx];
                const ubInfo = pipelineUbInfo[currentPipelineIdx];
                const sbInfo = pipelineSbInfo[currentPipelineIdx];

                // Bind group 0: Uniforms
                if (layouts && layouts[0] && uniformBuffer && ubInfo && ubInfo.count > 0) {
                    // Create bind group key
                    const key = `ub-${currentPipelineIdx}-${uniformBufIdx}`;

                    // Get or create bind group (created once per pipeline+buffer combo)
                    let bindGroup = pipelineUniformBindGroups.get(key);
                    if (!bindGroup) {
                        // Create bind group entries for each uniform block
                        const entries: GPUBindGroupEntry[] = [];
                        for (let i = 0; i < ubInfo.count; i++) {
                            entries.push({
                                binding: i,
                                resource: {
                                    buffer: uniformBuffer,
                                    // Size must be aligned to 256 for dynamic offset buffers
                                    size: Math.max(ubInfo.sizes[i], 256),
                                },
                            });
                        }

                        bindGroup = renderer.device.createBindGroup({
                            layout: layouts[0],
                            entries,
                        });
                        pipelineUniformBindGroups.set(key, bindGroup);
                    }

                    // Read uniform offsets from memory (only the ones this pipeline uses)
                    const allOffsets = new Uint32Array(memory.buffer, ubOffsetsPtr, ubCount);
                    const dynamicOffsets: number[] = [];
                    for (let i = 0; i < ubInfo.count; i++) {
                        dynamicOffsets.push(allOffsets[i]);
                    }

                    // Set bind group with dynamic offsets
                    currentPass.setBindGroup(0, bindGroup, dynamicOffsets);
                }

                // Bind group 1: Storage buffers
                if (layouts && layouts[1] && sbInfo && sbInfo.count > 0 && sbCount > 0) {
                    // Read storage buffer indices from memory
                    const sbIndices = new Uint32Array(memory.buffer, sbIndicesPtr, sbCount);

                    // Create bind group key from storage buffer indices
                    const key = `sb-${currentPipelineIdx}-${Array.from(sbIndices).join("-")}`;

                    // Get or create bind group
                    let bindGroup = pipelineUniformBindGroups.get(key);
                    if (!bindGroup) {
                        const entries: GPUBindGroupEntry[] = [];
                        for (let i = 0; i < sbCount; i++) {
                            const storageBuffer = buffers[sbIndices[i]];
                            if (storageBuffer) {
                                entries.push({
                                    binding: i,
                                    resource: {
                                        buffer: storageBuffer,
                                    },
                                });
                            }
                        }

                        if (entries.length > 0) {
                            bindGroup = renderer.device.createBindGroup({
                                layout: layouts[1],
                                entries,
                            });
                            pipelineUniformBindGroups.set(key, bindGroup);
                        }
                    }

                    if (bindGroup) {
                        currentPass.setBindGroup(1, bindGroup);
                    }
                }
            }
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
