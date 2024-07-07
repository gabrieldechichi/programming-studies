/**
 * @param {string} wasmPath                          - Path to the WASM module to run
 * @param {?HTMLPreElement} consoleElement           - Optional console/pre element to append output to, in addition to the console
 * @param {any} extraForeignImports                  - Imports, in addition to the default runtime to provide the module
 * @param {?WasmMemoryInterface} wasmMemoryInterface - Optional memory to use instead of the defaults
 * @param {?int} intSize                             - Size (in bytes) of the integer type, should be 4 on `js_wasm32` and 8 on `js_wasm64p32`
 */
export function runWasm(wasmPath: string, consoleElement: HTMLPreElement | null, extraForeignImports: any, wasmMemoryInterface: WasmMemoryInterface | null, intSize?: int | null): Promise<void>;
export class WasmMemoryInterface {
    memory: any;
    exports: any;
    listenerMap: {};
    intSize: number;
    setIntSize(size: any): void;
    setMemory(memory: any): void;
    setExports(exports: any): void;
    get mem(): DataView;
    loadF32Array(addr: any, len: any): Float32Array;
    loadF64Array(addr: any, len: any): Float64Array;
    loadU32Array(addr: any, len: any): Uint32Array;
    loadI32Array(addr: any, len: any): Int32Array;
    loadU8(addr: any): number;
    loadI8(addr: any): number;
    loadU16(addr: any): number;
    loadI16(addr: any): number;
    loadU32(addr: any): number;
    loadI32(addr: any): number;
    loadU64(addr: any): number;
    loadI64(addr: any): number;
    loadF32(addr: any): number;
    loadF64(addr: any): number;
    loadInt(addr: any): number;
    loadUint(addr: any): number;
    loadPtr(addr: any): number;
    loadB32(addr: any): boolean;
    loadBytes(ptr: any, len: any): Uint8Array;
    loadString(ptr: any, len: any): string;
    loadCstring(ptr: any): string;
    storeU8(addr: any, value: any): void;
    storeI8(addr: any, value: any): void;
    storeU16(addr: any, value: any): void;
    storeI16(addr: any, value: any): void;
    storeU32(addr: any, value: any): void;
    storeI32(addr: any, value: any): void;
    storeU64(addr: any, value: any): void;
    storeI64(addr: any, value: any): void;
    storeF32(addr: any, value: any): void;
    storeF64(addr: any, value: any): void;
    storeInt(addr: any, value: any): void;
    storeUint(addr: any, value: any): void;
    storeString(addr: any, value: any): number;
}
export class WebGLInterface {
    constructor(wasmMemoryInterface: any);
    wasmMemoryInterface: any;
    ctxElement: any;
    ctx: any;
    ctxVersion: number;
    counter: number;
    lastError: number;
    buffers: any[];
    mappedBuffers: {};
    programs: any[];
    framebuffers: any[];
    renderbuffers: any[];
    textures: any[]; uniforms: any[];
    shaders: any[];
    vaos: any[];
    contexts: any[];
    currentContext: any;
    offscreenCanvases: {};
    timerQueriesEXT: any[];
    queries: any[];
    samplers: any[];
    transformFeedbacks: any[];
    syncs: any[];
    programInfos: {};
    get mem(): any;
    setCurrentContext(element: any, contextSettings: any): boolean;
    assertWebGL2(): void;
    getNewId(table: any): number;
    recordError(errorCode: any): void;
    populateUniformTable(program: any): void;
    getSource(shader: any, strings_ptr: any, strings_length: any): string;
    getWebGL1Interface(): {
        SetCurrentContextById: (name_ptr: any, name_len: any) => boolean;
        CreateCurrentContextById: (name_ptr: any, name_len: any, attributes: any) => boolean;
        GetCurrentContextAttributes: () => number;
        DrawingBufferWidth: () => any;
        DrawingBufferHeight: () => any;
        IsExtensionSupported: (name_ptr: any, name_len: any) => boolean;
        GetError: () => any;
        GetWebGLVersion: (major_ptr: any, minor_ptr: any) => void;
        GetESVersion: (major_ptr: any, minor_ptr: any) => void;
        ActiveTexture: (x: any) => void;
        AttachShader: (program: any, shader: any) => void;
        BindAttribLocation: (program: any, index: any, name_ptr: any, name_len: any) => void;
        BindBuffer: (target: any, buffer: any) => void;
        BindFramebuffer: (target: any, framebuffer: any) => void;
        BindTexture: (target: any, texture: any) => void;
        BlendColor: (red: any, green: any, blue: any, alpha: any) => void;
        BlendEquation: (mode: any) => void;
        BlendFunc: (sfactor: any, dfactor: any) => void;
        BlendFuncSeparate: (srcRGB: any, dstRGB: any, srcAlpha: any, dstAlpha: any) => void;
        BufferData: (target: any, size: any, data: any, usage: any) => void;
        BufferSubData: (target: any, offset: any, size: any, data: any) => void;
        Clear: (x: any) => void;
        ClearColor: (r: any, g: any, b: any, a: any) => void;
        ClearDepth: (x: any) => void;
        ClearStencil: (x: any) => void;
        ColorMask: (r: any, g: any, b: any, a: any) => void;
        CompileShader: (shader: any) => void;
        CompressedTexImage2D: (target: any, level: any, internalformat: any, width: any, height: any, border: any, imageSize: any, data: any) => void;
        CompressedTexSubImage2D: (target: any, level: any, xoffset: any, yoffset: any, width: any, height: any, format: any, imageSize: any, data: any) => void;
        CopyTexImage2D: (target: any, level: any, internalformat: any, x: any, y: any, width: any, height: any, border: any) => void;
        CopyTexSubImage2D: (target: any, level: any, xoffset: any, yoffset: any, x: any, y: any, width: any, height: any) => void;
        CreateBuffer: () => number;
        CreateFramebuffer: () => number;
        CreateProgram: () => number;
        CreateRenderbuffer: () => number;
        CreateShader: (shaderType: any) => number;
        CreateTexture: () => number;
        CullFace: (mode: any) => void;
        DeleteBuffer: (id: any) => void;
        DeleteFramebuffer: (id: any) => void;
        DeleteProgram: (id: any) => void;
        DeleteRenderbuffer: (id: any) => void;
        DeleteShader: (id: any) => void;
        DeleteTexture: (id: any) => void;
        DepthFunc: (func: any) => void;
        DepthMask: (flag: any) => void;
        DepthRange: (zNear: any, zFar: any) => void;
        DetachShader: (program: any, shader: any) => void;
        Disable: (cap: any) => void;
        DisableVertexAttribArray: (index: any) => void;
        DrawArrays: (mode: any, first: any, count: any) => void;
        DrawElements: (mode: any, count: any, type: any, indices: any) => void;
        Enable: (cap: any) => void;
        EnableVertexAttribArray: (index: any) => void;
        Finish: () => void;
        Flush: () => void;
        FramebufferRenderbuffer: (target: any, attachment: any, renderbuffertarget: any, renderbuffer: any) => void;
        FramebufferTexture2D: (target: any, attachment: any, textarget: any, texture: any, level: any) => void;
        FrontFace: (mode: any) => void;
        GenerateMipmap: (target: any) => void;
        GetAttribLocation: (program: any, name_ptr: any, name_len: any) => any;
        GetParameter: (pname: any) => any;
        GetProgramParameter: (program: any, pname: any) => any;
        GetProgramInfoLog: (program: any, buf_ptr: any, buf_len: any, length_ptr: any) => void;
        GetShaderInfoLog: (shader: any, buf_ptr: any, buf_len: any, length_ptr: any) => void;
        GetShaderiv: (shader: any, pname: any, p: any) => void;
        GetUniformLocation: (program: any, name_ptr: any, name_len: any) => any;
        GetVertexAttribOffset: (index: any, pname: any) => any;
        Hint: (target: any, mode: any) => void;
        IsBuffer: (buffer: any) => any;
        IsEnabled: (cap: any) => any;
        IsFramebuffer: (framebuffer: any) => any;
        IsProgram: (program: any) => any;
        IsRenderbuffer: (renderbuffer: any) => any;
        IsShader: (shader: any) => any;
        IsTexture: (texture: any) => any;
        LineWidth: (width: any) => void;
        LinkProgram: (program: any) => void;
        PixelStorei: (pname: any, param: any) => void;
        PolygonOffset: (factor: any, units: any) => void;
        ReadnPixels: (x: any, y: any, width: any, height: any, format: any, type: any, bufSize: any, data: any) => void;
        RenderbufferStorage: (target: any, internalformat: any, width: any, height: any) => void;
        SampleCoverage: (value: any, invert: any) => void;
        Scissor: (x: any, y: any, width: any, height: any) => void;
        ShaderSource: (shader: any, strings_ptr: any, strings_length: any) => void;
        StencilFunc: (func: any, ref: any, mask: any) => void;
        StencilFuncSeparate: (face: any, func: any, ref: any, mask: any) => void;
        StencilMask: (mask: any) => void;
        StencilMaskSeparate: (face: any, mask: any) => void;
        StencilOp: (fail: any, zfail: any, zpass: any) => void;
        StencilOpSeparate: (face: any, fail: any, zfail: any, zpass: any) => void;
        TexImage2D: (target: any, level: any, internalformat: any, width: any, height: any, border: any, format: any, type: any, size: any, data: any) => void;
        TexParameterf: (target: any, pname: any, param: any) => void;
        TexParameteri: (target: any, pname: any, param: any) => void;
        TexSubImage2D: (target: any, level: any, xoffset: any, yoffset: any, width: any, height: any, format: any, type: any, size: any, data: any) => void;
        Uniform1f: (location: any, v0: any) => void;
        Uniform2f: (location: any, v0: any, v1: any) => void;
        Uniform3f: (location: any, v0: any, v1: any, v2: any) => void;
        Uniform4f: (location: any, v0: any, v1: any, v2: any, v3: any) => void;
        Uniform1i: (location: any, v0: any) => void;
        Uniform2i: (location: any, v0: any, v1: any) => void;
        Uniform3i: (location: any, v0: any, v1: any, v2: any) => void;
        Uniform4i: (location: any, v0: any, v1: any, v2: any, v3: any) => void;
        UniformMatrix2fv: (location: any, addr: any) => void;
        UniformMatrix3fv: (location: any, addr: any) => void;
        UniformMatrix4fv: (location: any, addr: any) => void;
        UseProgram: (program: any) => void;
        ValidateProgram: (program: any) => void;
        VertexAttrib1f: (index: any, x: any) => void;
        VertexAttrib2f: (index: any, x: any, y: any) => void;
        VertexAttrib3f: (index: any, x: any, y: any, z: any) => void;
        VertexAttrib4f: (index: any, x: any, y: any, z: any, w: any) => void;
        VertexAttribPointer: (index: any, size: any, type: any, normalized: any, stride: any, ptr: any) => void;
        Viewport: (x: any, y: any, w: any, h: any) => void;
    };
    getWebGL2Interface(): {
        CopyBufferSubData: (readTarget: any, writeTarget: any, readOffset: any, writeOffset: any, size: any) => void;
        GetBufferSubData: (target: any, srcByteOffset: any, dst_buffer_ptr: any, dst_buffer_len: any, dstOffset: any, length: any) => void;
        BlitFramebuffer: (srcX0: any, srcY0: any, srcX1: any, srcY1: any, dstX0: any, dstY0: any, dstX1: any, dstY1: any, mask: any, filter: any) => void;
        FramebufferTextureLayer: (target: any, attachment: any, texture: any, level: any, layer: any) => void;
        InvalidateFramebuffer: (target: any, attachments_ptr: any, attachments_len: any) => void;
        InvalidateSubFramebuffer: (target: any, attachments_ptr: any, attachments_len: any, x: any, y: any, width: any, height: any) => void;
        ReadBuffer: (src: any) => void;
        RenderbufferStorageMultisample: (target: any, samples: any, internalformat: any, width: any, height: any) => void;
        TexStorage3D: (target: any, levels: any, internalformat: any, width: any, height: any, depth: any) => void;
        TexImage3D: (target: any, level: any, internalformat: any, width: any, height: any, depth: any, border: any, format: any, type: any, size: any, data: any) => void;
        TexSubImage3D: (target: any, level: any, xoffset: any, yoffset: any, zoffset: any, width: any, height: any, depth: any, format: any, type: any, size: any, data: any) => void;
        CompressedTexImage3D: (target: any, level: any, internalformat: any, width: any, height: any, depth: any, border: any, imageSize: any, data: any) => void;
        CompressedTexSubImage3D: (target: any, level: any, xoffset: any, yoffset: any, zoffset: any, width: any, height: any, depth: any, format: any, imageSize: any, data: any) => void;
        CopyTexSubImage3D: (target: any, level: any, xoffset: any, yoffset: any, zoffset: any, x: any, y: any, width: any, height: any) => void;
        GetFragDataLocation: (program: any, name_ptr: any, name_len: any) => any;
        Uniform1ui: (location: any, v0: any) => void;
        Uniform2ui: (location: any, v0: any, v1: any) => void;
        Uniform3ui: (location: any, v0: any, v1: any, v2: any) => void;
        Uniform4ui: (location: any, v0: any, v1: any, v2: any, v3: any) => void;
        UniformMatrix3x2fv: (location: any, addr: any) => void;
        UniformMatrix4x2fv: (location: any, addr: any) => void;
        UniformMatrix2x3fv: (location: any, addr: any) => void;
        UniformMatrix4x3fv: (location: any, addr: any) => void;
        UniformMatrix2x4fv: (location: any, addr: any) => void;
        UniformMatrix3x4fv: (location: any, addr: any) => void;
        VertexAttribI4i: (index: any, x: any, y: any, z: any, w: any) => void;
        VertexAttribI4ui: (index: any, x: any, y: any, z: any, w: any) => void;
        VertexAttribIPointer: (index: any, size: any, type: any, stride: any, offset: any) => void;
        VertexAttribDivisor: (index: any, divisor: any) => void;
        DrawArraysInstanced: (mode: any, first: any, count: any, instanceCount: any) => void;
        DrawElementsInstanced: (mode: any, count: any, type: any, offset: any, instanceCount: any) => void;
        DrawRangeElements: (mode: any, start: any, end: any, count: any, type: any, offset: any) => void;
        DrawBuffers: (buffers_ptr: any, buffers_len: any) => void;
        ClearBufferfv: (buffer: any, drawbuffer: any, values_ptr: any, values_len: any) => void;
        ClearBufferiv: (buffer: any, drawbuffer: any, values_ptr: any, values_len: any) => void;
        ClearBufferuiv: (buffer: any, drawbuffer: any, values_ptr: any, values_len: any) => void;
        ClearBufferfi: (buffer: any, drawbuffer: any, depth: any, stencil: any) => void;
        CreateQuery: () => number;
        DeleteQuery: (id: any) => void;
        IsQuery: (query: any) => any;
        BeginQuery: (target: any, query: any) => void;
        EndQuery: (target: any) => void;
        GetQuery: (target: any, pname: any) => any;
        CreateSampler: () => number;
        DeleteSampler: (id: any) => void;
        IsSampler: (sampler: any) => any;
        BindSampler: (unit: any, sampler: any) => void;
        SamplerParameteri: (sampler: any, pname: any, param: any) => void;
        SamplerParameterf: (sampler: any, pname: any, param: any) => void;
        FenceSync: (condition: any, flags: any) => number;
        IsSync: (sync: any) => any;
        DeleteSync: (id: any) => void;
        ClientWaitSync: (sync: any, flags: any, timeout: any) => any;
        WaitSync: (sync: any, flags: any, timeout: any) => void;
        CreateTransformFeedback: () => number;
        DeleteTransformFeedback: (id: any) => void;
        IsTransformFeedback: (tf: any) => any;
        BindTransformFeedback: (target: any, tf: any) => void;
        BeginTransformFeedback: (primitiveMode: any) => void;
        EndTransformFeedback: () => void;
        TransformFeedbackVaryings: (program: any, varyings_ptr: any, varyings_len: any, bufferMode: any) => void;
        PauseTransformFeedback: () => void;
        ResumeTransformFeedback: () => void;
        BindBufferBase: (target: any, index: any, buffer: any) => void;
        BindBufferRange: (target: any, index: any, buffer: any, offset: any, size: any) => void;
        GetUniformBlockIndex: (program: any, uniformBlockName_ptr: any, uniformBlockName_len: any) => any;
        GetActiveUniformBlockName: (program: any, uniformBlockIndex: any, buf_ptr: any, buf_len: any, length_ptr: any) => void;
        UniformBlockBinding: (program: any, uniformBlockIndex: any, uniformBlockBinding: any) => void;
        CreateVertexArray: () => number;
        DeleteVertexArray: (id: any) => void;
        IsVertexArray: (vertexArray: any) => any;
        BindVertexArray: (vertexArray: any) => void;
    };
}
