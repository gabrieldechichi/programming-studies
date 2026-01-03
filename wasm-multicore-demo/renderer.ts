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
const textures: (GPUTexture | null)[] = [];
const samplers: (GPUSampler | null)[] = [];

// Render targets: color texture + depth texture
interface RenderTarget {
    colorTexture: GPUTexture;
    depthTexture: GPUTexture;
    format: GPUTextureFormat;
}
const renderTargets: (RenderTarget | null)[] = [];

// Scene render format (used for pipeline creation) - set when first HDR target is created
let sceneRenderFormat: GPUTextureFormat = "bgra8unorm";

// Blit resources (created lazily)
let blitPipeline: GPURenderPipeline | null = null;
let blitSampler: GPUSampler | null = null;
let blitBindGroupLayout: GPUBindGroupLayout | null = null;


// Per-pipeline bind group layouts: [0] = uniforms (group 0), [1] = storage (group 1), [2] = textures (group 2)
const pipelineBindGroupLayouts: [GPUBindGroupLayout | null, GPUBindGroupLayout | null, GPUBindGroupLayout | null][] = [];

// Per-pipeline uniform block info (sizes needed for bind group creation)
const pipelineUbInfo: { count: number; sizes: number[] }[] = [];

// Per-pipeline storage buffer info (count needed for bind group creation)
const pipelineSbInfo: { count: number }[] = [];

// Per-pipeline texture binding info (sampler and texture binding indices)
const pipelineTexInfo: { count: number; samplerBindings: number[]; textureBindings: number[] }[] = [];

// Per-pipeline bind groups for dynamic uniforms (created once, reused with different offsets)
const pipelineUniformBindGroups: Map<string, GPUBindGroup> = new Map();

// Current frame state
let currentEncoder: GPUCommandEncoder | null = null;
let currentPass: GPURenderPassEncoder | null = null;
let currentPipelineIdx: number = -1;

let renderer: Renderer | null = null;

export function resizeRenderer(width: number, height: number): void {
    if (!renderer) return;

    // Destroy old depth texture
    renderer.depthTexture.destroy();

    // Create new depth texture with updated dimensions
    renderer.depthTexture = renderer.device.createTexture({
        size: [width, height],
        format: "depth24plus",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });
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

    const format: GPUTextureFormat = "bgra8unorm"; // No automatic gamma correction
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

const TEXTURE_FORMATS: GPUTextureFormat[] = [
    "rgba8unorm",  // GPU_TEXTURE_FORMAT_RGBA8
    "rgba16float", // GPU_TEXTURE_FORMAT_RGBA16F
];

// GPU_CULL_BACK = 0, GPU_CULL_NONE = 1, GPU_CULL_FRONT = 2
const CULL_MODES: GPUCullMode[] = [
    "back",  // GPU_CULL_BACK (default)
    "none",  // GPU_CULL_NONE
    "front", // GPU_CULL_FRONT
];

// GPU_FACE_CCW = 0, GPU_FACE_CW = 1
const FRONT_FACES: GPUFrontFace[] = [
    "ccw", // GPU_FACE_CCW (default)
    "cw",  // GPU_FACE_CW
];

// GPU_COMPARE_LESS = 0, GPU_COMPARE_NEVER = 1, etc.
const COMPARE_FUNCS: GPUCompareFunction[] = [
    "less",          // GPU_COMPARE_LESS (default)
    "never",         // GPU_COMPARE_NEVER
    "equal",         // GPU_COMPARE_EQUAL
    "less-equal",    // GPU_COMPARE_LESS_EQUAL
    "greater",       // GPU_COMPARE_GREATER
    "not-equal",     // GPU_COMPARE_NOT_EQUAL
    "greater-equal", // GPU_COMPARE_GREATER_EQUAL
    "always",        // GPU_COMPARE_ALWAYS
];

// GPU_BLEND_ZERO = 0, GPU_BLEND_ONE = 1, etc.
const BLEND_FACTORS: GPUBlendFactor[] = [
    "zero",              // GPU_BLEND_ZERO
    "one",               // GPU_BLEND_ONE
    "src",               // GPU_BLEND_SRC_COLOR
    "one-minus-src",     // GPU_BLEND_ONE_MINUS_SRC_COLOR
    "src-alpha",         // GPU_BLEND_SRC_ALPHA
    "one-minus-src-alpha", // GPU_BLEND_ONE_MINUS_SRC_ALPHA
    "dst",               // GPU_BLEND_DST_COLOR
    "one-minus-dst",     // GPU_BLEND_ONE_MINUS_DST_COLOR
    "dst-alpha",         // GPU_BLEND_DST_ALPHA
    "one-minus-dst-alpha", // GPU_BLEND_ONE_MINUS_DST_ALPHA
];

// GPU_BLEND_OP_ADD = 0, etc.
const BLEND_OPS: GPUBlendOperation[] = [
    "add",              // GPU_BLEND_OP_ADD
    "subtract",         // GPU_BLEND_OP_SUBTRACT
    "reverse-subtract", // GPU_BLEND_OP_REVERSE_SUBTRACT
];

const BLIT_SHADER = `
@group(0) @binding(0) var blitSampler: sampler;
@group(0) @binding(1) var blitTexture: texture_2d<f32>;

const EXPOSURE: f32 = 1.0;
const CONTRAST: f32 = 1.0;
const SATURATION: f32 = 1.1;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var uv = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(2.0, 1.0),
        vec2<f32>(0.0, -1.0)
    );
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.uv = uv[vertexIndex];
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    var color = textureSample(blitTexture, blitSampler, input.uv).rgb;

    // Linear tonemapping
    color = clamp(color, vec3<f32>(0.0), vec3<f32>(1.0));

    // Contrast (around mid-gray)
    // color = (color - 0.5) * CONTRAST + 0.5;

    // Saturation
    // let luminance = dot(color, vec3<f32>(0.2126, 0.7152, 0.0722));
    // color = mix(vec3<f32>(luminance), color, SATURATION);

    // Gamma correction
    // color = pow(color, vec3<f32>(1.0 / 1.5));
    
    return vec4<f32>(clamp(color, vec3<f32>(0.0), vec3<f32>(1.0)), 1.0);
}
`;

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

            // WebGPU requires buffer size to be a multiple of 4 when mappedAtCreation is true
            const alignedSize = Math.ceil(size / 4) * 4;

            const buffer = renderer.device.createBuffer({
                size: alignedSize,
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

        js_gpu_make_texture_data: (idx: number, width: number, height: number, dataPtr: number) => {
            if (!renderer) return;

            const texture = renderer.device.createTexture({
                size: [width, height],
                format: "rgba8unorm",
                usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
            });

            const data = new Uint8Array(memory.buffer, dataPtr, width * height * 4);
            renderer.device.queue.writeTexture(
                { texture },
                data,
                { bytesPerRow: width * 4 },
                [width, height]
            );

            const sampler = renderer.device.createSampler({
                minFilter: "linear",
                magFilter: "linear",
            });

            textures[idx] = texture;
            samplers[idx] = sampler;
        },

        js_gpu_load_texture: (idx: number, pathPtr: number, pathLen: number) => {
            if (!renderer) return;

            const path = readString(memory, pathPtr, pathLen);
            textures[idx] = null;
            samplers[idx] = null;

            fetch(path)
                .then(response => response.blob())
                .then(blob => createImageBitmap(blob))
                .then(bitmap => {
                    if (!renderer) return;

                    const texture = renderer.device.createTexture({
                        size: [bitmap.width, bitmap.height],
                        format: "rgba8unorm",
                        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
                    });

                    renderer.device.queue.copyExternalImageToTexture(
                        { source: bitmap },
                        { texture },
                        [bitmap.width, bitmap.height]
                    );

                    const sampler = renderer.device.createSampler({
                        minFilter: "linear",
                        magFilter: "linear",
                        mipmapFilter: "linear",
                    });

                    textures[idx] = texture;
                    samplers[idx] = sampler;
                })
                .catch(err => {
                    console.error(`[GPU] Failed to load texture ${path}:`, err);
                });
        },

        js_gpu_texture_is_ready: (idx: number): number => {
            return textures[idx] !== null ? 1 : 0;
        },

        js_gpu_destroy_texture: (idx: number) => {
            const texture = textures[idx];
            if (texture) {
                texture.destroy();
                textures[idx] = null;
                samplers[idx] = null;
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
            texCount: number,
            texStagesPtr: number,
            texSamplerBindingsPtr: number,
            texTextureBindingsPtr: number,
            primitive: number,
            cullMode: number,
            faceWinding: number,
            depthCompare: number,
            depthTest: number,
            depthWrite: number,
            blendEnabled: number,
            blendSrc: number,
            blendDst: number,
            blendOp: number,
            blendSrcAlpha: number,
            blendDstAlpha: number,
            blendOpAlpha: number
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

            pipelineSbInfo[idx] = { count: sbCount };

            const texStages = new Uint32Array(memory.buffer, texStagesPtr, texCount);
            const texSamplerBindings = new Uint32Array(memory.buffer, texSamplerBindingsPtr, texCount);
            const texTextureBindings = new Uint32Array(memory.buffer, texTextureBindingsPtr, texCount);

            const samplerBindingsArr: number[] = [];
            const textureBindingsArr: number[] = [];
            for (let i = 0; i < texCount; i++) {
                samplerBindingsArr.push(texSamplerBindings[i]);
                textureBindingsArr.push(texTextureBindings[i]);
            }
            pipelineTexInfo[idx] = { count: texCount, samplerBindings: samplerBindingsArr, textureBindings: textureBindingsArr };

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

            const textureLayoutEntries: GPUBindGroupLayoutEntry[] = [];
            for (let i = 0; i < texCount; i++) {
                textureLayoutEntries.push({
                    binding: texSamplerBindings[i],
                    visibility: stageToGPU(texStages[i]),
                    sampler: {},
                });
                textureLayoutEntries.push({
                    binding: texTextureBindings[i],
                    visibility: stageToGPU(texStages[i]),
                    texture: {},
                });
            }

            const textureBindGroupLayout = texCount > 0
                ? renderer.device.createBindGroupLayout({ entries: textureLayoutEntries })
                : null;

            const bindGroupLayouts: GPUBindGroupLayout[] = [];
            const emptyLayout = renderer.device.createBindGroupLayout({ entries: [] });

            if (uniformBindGroupLayout) {
                bindGroupLayouts.push(uniformBindGroupLayout);
            } else if (storageBindGroupLayout || textureBindGroupLayout) {
                bindGroupLayouts.push(emptyLayout);
            }

            if (storageBindGroupLayout) {
                bindGroupLayouts.push(storageBindGroupLayout);
            } else if (textureBindGroupLayout) {
                bindGroupLayouts.push(emptyLayout);
            }

            if (textureBindGroupLayout) {
                bindGroupLayouts.push(textureBindGroupLayout);
            }

            const pipelineLayout = renderer.device.createPipelineLayout({
                bindGroupLayouts: bindGroupLayouts,
            });

            const colorTarget: GPUColorTargetState = {
                format: sceneRenderFormat,
            };

            if (blendEnabled) {
                colorTarget.blend = {
                    color: {
                        srcFactor: BLEND_FACTORS[blendSrc] || "one",
                        dstFactor: BLEND_FACTORS[blendDst] || "zero",
                        operation: BLEND_OPS[blendOp] || "add",
                    },
                    alpha: {
                        srcFactor: BLEND_FACTORS[blendSrcAlpha] || "one",
                        dstFactor: BLEND_FACTORS[blendDstAlpha] || "zero",
                        operation: BLEND_OPS[blendOpAlpha] || "add",
                    },
                };
            }

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
                    targets: [colorTarget],
                },
                primitive: {
                    topology: PRIMITIVE_TOPOLOGIES[primitive],
                    cullMode: CULL_MODES[cullMode] || "back",
                    frontFace: FRONT_FACES[faceWinding] || "ccw",
                },
                depthStencil: {
                    format: "depth24plus",
                    depthWriteEnabled: depthTest ? !!depthWrite : false,
                    depthCompare: depthTest ? (COMPARE_FUNCS[depthCompare] || "less") : "always",
                },
            });

            pipelines[idx] = pipeline;
            pipelineBindGroupLayouts[idx] = [uniformBindGroupLayout, storageBindGroupLayout, textureBindGroupLayout];
        },

        js_gpu_destroy_pipeline: (handleIdx: number) => {
            pipelines[handleIdx] = null;
            pipelineBindGroupLayouts[handleIdx] = [null, null, null];
            delete pipelineUbInfo[handleIdx];
            delete pipelineSbInfo[handleIdx];
            delete pipelineTexInfo[handleIdx];
        },

        js_gpu_begin_pass: (r: number, g: number, b: number, a: number, depth: number, rtIdx: number) => {
            if (!renderer) return;

            currentEncoder = renderer.device.createCommandEncoder();

            let colorView: GPUTextureView;
            let depthView: GPUTextureView;

            if (rtIdx !== 0xFFFFFFFF && renderTargets[rtIdx]) {
                const rt = renderTargets[rtIdx]!;
                colorView = rt.colorTexture.createView();
                depthView = rt.depthTexture.createView();
            } else {
                colorView = renderer.context.getCurrentTexture().createView();
                depthView = renderer.depthTexture.createView();
            }

            currentPass = currentEncoder.beginRenderPass({
                colorAttachments: [
                    {
                        view: colorView,
                        clearValue: { r, g, b, a },
                        loadOp: "clear",
                        storeOp: "store",
                    },
                ],
                depthStencilAttachment: {
                    view: depthView,
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
            sbIndicesPtr: number,
            texCount: number,
            texIndicesPtr: number
        ) => {
            if (!currentPass || !renderer) return;

            const vbIndices = new Uint32Array(memory.buffer, vbIndicesPtr, vbCount);

            for (let i = 0; i < vbCount; i++) {
                const buffer = buffers[vbIndices[i]];
                if (buffer) {
                    currentPass.setVertexBuffer(i, buffer);
                }
            }

            const indexBuffer = buffers[ibIdx];
            if (indexBuffer) {
                currentPass.setIndexBuffer(indexBuffer, INDEX_FORMATS[ibFormat]);
            }

            if (currentPipelineIdx >= 0) {
                const layouts = pipelineBindGroupLayouts[currentPipelineIdx];
                const uniformBuffer = buffers[uniformBufIdx];
                const ubInfo = pipelineUbInfo[currentPipelineIdx];
                const sbInfo = pipelineSbInfo[currentPipelineIdx];
                const texInfo = pipelineTexInfo[currentPipelineIdx];

                if (layouts && layouts[0] && uniformBuffer && ubInfo && ubInfo.count > 0) {
                    const key = `ub-${currentPipelineIdx}-${uniformBufIdx}`;

                    let bindGroup = pipelineUniformBindGroups.get(key);
                    if (!bindGroup) {
                        const entries: GPUBindGroupEntry[] = [];
                        for (let i = 0; i < ubInfo.count; i++) {
                            entries.push({
                                binding: i,
                                resource: {
                                    buffer: uniformBuffer,
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

                    const allOffsets = new Uint32Array(memory.buffer, ubOffsetsPtr, ubCount);
                    const dynamicOffsets: number[] = [];
                    for (let i = 0; i < ubInfo.count; i++) {
                        dynamicOffsets.push(allOffsets[i]);
                    }

                    currentPass.setBindGroup(0, bindGroup, dynamicOffsets);
                }

                if (layouts && layouts[1] && sbInfo && sbInfo.count > 0 && sbCount > 0) {
                    const sbIndices = new Uint32Array(memory.buffer, sbIndicesPtr, sbCount);

                    const key = `sb-${currentPipelineIdx}-${Array.from(sbIndices).join("-")}`;

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

                if (layouts && layouts[2] && texInfo && texInfo.count > 0 && texCount > 0) {
                    const texIndices = new Uint32Array(memory.buffer, texIndicesPtr, texCount);

                    const key = `tex-${currentPipelineIdx}-${Array.from(texIndices).join("-")}`;

                    let bindGroup = pipelineUniformBindGroups.get(key);
                    if (!bindGroup) {
                        const entries: GPUBindGroupEntry[] = [];
                        for (let i = 0; i < texCount; i++) {
                            const sampler = samplers[texIndices[i]];
                            const texture = textures[texIndices[i]];
                            if (sampler && texture) {
                                entries.push({
                                    binding: texInfo.samplerBindings[i],
                                    resource: sampler,
                                });
                                entries.push({
                                    binding: texInfo.textureBindings[i],
                                    resource: texture.createView(),
                                });
                            }
                        }

                        if (entries.length > 0) {
                            bindGroup = renderer.device.createBindGroup({
                                layout: layouts[2],
                                entries,
                            });
                            pipelineUniformBindGroups.set(key, bindGroup);
                        }
                    }

                    if (bindGroup) {
                        currentPass.setBindGroup(2, bindGroup);
                    }
                }
            }
        },

        js_gpu_make_render_target: (idx: number, width: number, height: number, format: number) => {
            if (!renderer) return;

            const gpuFormat = TEXTURE_FORMATS[format];
            sceneRenderFormat = gpuFormat;

            const colorTexture = renderer.device.createTexture({
                size: [width, height],
                format: gpuFormat,
                usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
            });

            const depthTexture = renderer.device.createTexture({
                size: [width, height],
                format: "depth24plus",
                usage: GPUTextureUsage.RENDER_ATTACHMENT,
            });

            renderTargets[idx] = {
                colorTexture,
                depthTexture,
                format: gpuFormat,
            };
        },

        js_gpu_resize_render_target: (idx: number, width: number, height: number) => {
            if (!renderer) return;
            const rt = renderTargets[idx];
            if (!rt) return;

            rt.colorTexture.destroy();
            rt.depthTexture.destroy();

            rt.colorTexture = renderer.device.createTexture({
                size: [width, height],
                format: rt.format,
                usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
            });

            rt.depthTexture = renderer.device.createTexture({
                size: [width, height],
                format: "depth24plus",
                usage: GPUTextureUsage.RENDER_ATTACHMENT,
            });
        },

        js_gpu_destroy_render_target: (idx: number) => {
            const rt = renderTargets[idx];
            if (rt) {
                rt.colorTexture.destroy();
                rt.depthTexture.destroy();
                renderTargets[idx] = null;
            }
        },

        js_gpu_blit_to_screen: (rtIdx: number) => {
            if (!renderer || !currentEncoder) return;
            const rt = renderTargets[rtIdx];
            if (!rt) return;

            // Create blit resources lazily
            if (!blitPipeline) {
                const shaderModule = renderer.device.createShaderModule({ code: BLIT_SHADER });

                blitBindGroupLayout = renderer.device.createBindGroupLayout({
                    entries: [
                        { binding: 0, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
                        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} },
                    ],
                });

                blitSampler = renderer.device.createSampler({
                    minFilter: "linear",
                    magFilter: "linear",
                });

                blitPipeline = renderer.device.createRenderPipeline({
                    layout: renderer.device.createPipelineLayout({
                        bindGroupLayouts: [blitBindGroupLayout],
                    }),
                    vertex: {
                        module: shaderModule,
                        entryPoint: "vs_main",
                    },
                    fragment: {
                        module: shaderModule,
                        entryPoint: "fs_main",
                        targets: [{ format: renderer.format }],
                    },
                    primitive: {
                        topology: "triangle-list",
                    },
                });
            }

            const bindGroup = renderer.device.createBindGroup({
                layout: blitBindGroupLayout!,
                entries: [
                    { binding: 0, resource: blitSampler! },
                    { binding: 1, resource: rt.colorTexture.createView() },
                ],
            });

            const textureView = renderer.context.getCurrentTexture().createView();

            const blitPass = currentEncoder.beginRenderPass({
                colorAttachments: [
                    {
                        view: textureView,
                        loadOp: "clear",
                        storeOp: "store",
                    },
                ],
            });

            blitPass.setPipeline(blitPipeline);
            blitPass.setBindGroup(0, bindGroup);
            blitPass.draw(3);
            blitPass.end();
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
