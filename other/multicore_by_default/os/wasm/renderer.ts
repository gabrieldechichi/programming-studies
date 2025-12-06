import { WasmMemoryInterface } from "./wasm_memory";

let cachedCompressedFormat: number | null = null;

const CompressedTextureFormat = {
  NONE: 0,
  DXT5: 1,
  ETC2: 2,
  ASTC: 3,
  ETC1: 4,
} as const;

function detectCompressedTextureFormat(gl: WebGL2RenderingContext): number {
  const s3tc = gl.getExtension("WEBGL_compressed_texture_s3tc");
  const s3tc_srgb = gl.getExtension("WEBGL_compressed_texture_s3tc_srgb");
  const etc = gl.getExtension("WEBGL_compressed_texture_etc");
  const etc1 = gl.getExtension("WEBGL_compressed_texture_etc1");
  const astc = gl.getExtension("WEBGL_compressed_texture_astc");
  const btc = gl.getExtension("EXT_texture_compression_bptc");
  console.log(btc);

  if (s3tc || s3tc_srgb) {
    return CompressedTextureFormat.DXT5;
  }
  if (etc) {
    return CompressedTextureFormat.ETC2;
  }
  if (astc) {
    return CompressedTextureFormat.ASTC;
  }
  if (etc1) {
    return CompressedTextureFormat.ETC1;
  }

  return CompressedTextureFormat.NONE;
}

function _os_get_compressed_texture_format(): number {
  if (cachedCompressedFormat === null) {
    throw new Error(
      "Compressed texture format not initialized. Call from renderer initialization first.",
    );
  }
  return cachedCompressedFormat;
}

type ShaderData = {
  program: WebGLProgram;
  uniform_locations: (WebGLUniformLocation | null)[][];
};

type TextureData = {
  texture: WebGLTexture;
  gl_target: number;
};

type Renderer = {
  samplers: (WebGLSampler | null)[];
  buffers: (WebGLBuffer | null)[];
  textures: (TextureData | null)[];
  shaders: (ShaderData | null)[];
  framebuffers: (WebGLFramebuffer | null)[];
  global_fbo: WebGLFramebuffer;
  whiteTexture: WebGLTexture;
  current_shader_id: number;
  current_vb_id: number;
  current_ib_id: number;
  draw_call_count: number;
  draw_call_counter: number;
  current_pass_target: string;
};

export function createWebGLRenderer(
  wasmMemory: WasmMemoryInterface,
  canvas: HTMLCanvasElement,
) {
  const gl = canvas.getContext("webgl2", {
    antialias: true,
    powerPreference: "high-performance",
    alpha: false,
    depth: true,
    stencil: false,
  });

  if (!gl) {
    throw new Error("WebGL2 not supported");
  }

  const ext = gl.getExtension("EXT_color_buffer_float");
  if (!ext) {
    console.warn(
      "EXT_color_buffer_float not supported, HDR rendering may fail",
    );
  }

  cachedCompressedFormat = detectCompressedTextureFormat(gl);
  const formatNames = ["NONE", "DXT5", "ETC2", "ASTC", "ETC1"];
  console.log(
    `Compressed texture format: ${formatNames[cachedCompressedFormat]}`,
  );

  const global_fbo = gl.createFramebuffer();
  if (!global_fbo) {
    throw new Error("Failed to create global framebuffer");
  }

  const whiteTexture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, whiteTexture);
  gl.texImage2D(
    gl.TEXTURE_2D,
    0,
    gl.RGBA,
    1,
    1,
    0,
    gl.RGBA,
    gl.UNSIGNED_BYTE,
    new Uint8Array([255, 255, 255, 255]),
  );
  gl.bindTexture(gl.TEXTURE_2D, null);

  const renderer: Renderer = {
    samplers: [],
    buffers: [],
    textures: [],
    shaders: [],
    framebuffers: [],
    global_fbo: global_fbo,
    whiteTexture: whiteTexture,
    current_shader_id: 0,
    current_vb_id: 0,
    current_ib_id: 0,
    draw_call_count: 0,
    draw_call_counter: 0,
    current_pass_target: "UNKNOWN",
  };

  function vertexFormatToGL(format: number): {
    components: number;
    gl_type: number;
    normalized: boolean;
  } {
    const SG_VERTEXFORMAT_INVALID = 0;
    const SG_VERTEXFORMAT_FLOAT = 1;
    const SG_VERTEXFORMAT_FLOAT2 = 2;
    const SG_VERTEXFORMAT_FLOAT3 = 3;
    const SG_VERTEXFORMAT_FLOAT4 = 4;
    const SG_VERTEXFORMAT_INT = 5;
    const SG_VERTEXFORMAT_INT2 = 6;
    const SG_VERTEXFORMAT_INT3 = 7;
    const SG_VERTEXFORMAT_INT4 = 8;
    const SG_VERTEXFORMAT_UINT = 9;
    const SG_VERTEXFORMAT_UINT2 = 10;
    const SG_VERTEXFORMAT_UINT3 = 11;
    const SG_VERTEXFORMAT_UINT4 = 12;
    const SG_VERTEXFORMAT_BYTE4 = 13;
    const SG_VERTEXFORMAT_BYTE4N = 14;
    const SG_VERTEXFORMAT_UBYTE4 = 15;
    const SG_VERTEXFORMAT_UBYTE4N = 16;
    const SG_VERTEXFORMAT_SHORT2 = 17;
    const SG_VERTEXFORMAT_SHORT2N = 18;
    const SG_VERTEXFORMAT_USHORT2 = 19;
    const SG_VERTEXFORMAT_USHORT2N = 20;
    const SG_VERTEXFORMAT_SHORT4 = 21;
    const SG_VERTEXFORMAT_SHORT4N = 22;
    const SG_VERTEXFORMAT_USHORT4 = 23;
    const SG_VERTEXFORMAT_USHORT4N = 24;
    const SG_VERTEXFORMAT_UINT10_N2 = 25;
    const SG_VERTEXFORMAT_HALF2 = 26;
    const SG_VERTEXFORMAT_HALF4 = 27;

    switch (format) {
      case SG_VERTEXFORMAT_FLOAT:
        return { components: 1, gl_type: gl.FLOAT, normalized: false };
      case SG_VERTEXFORMAT_FLOAT2:
        return { components: 2, gl_type: gl.FLOAT, normalized: false };
      case SG_VERTEXFORMAT_FLOAT3:
        return { components: 3, gl_type: gl.FLOAT, normalized: false };
      case SG_VERTEXFORMAT_FLOAT4:
        return { components: 4, gl_type: gl.FLOAT, normalized: false };
      case SG_VERTEXFORMAT_INT:
        return { components: 1, gl_type: gl.INT, normalized: false };
      case SG_VERTEXFORMAT_INT2:
        return { components: 2, gl_type: gl.INT, normalized: false };
      case SG_VERTEXFORMAT_INT3:
        return { components: 3, gl_type: gl.INT, normalized: false };
      case SG_VERTEXFORMAT_INT4:
        return { components: 4, gl_type: gl.INT, normalized: false };
      case SG_VERTEXFORMAT_UINT:
        return { components: 1, gl_type: gl.UNSIGNED_INT, normalized: false };
      case SG_VERTEXFORMAT_UINT2:
        return { components: 2, gl_type: gl.UNSIGNED_INT, normalized: false };
      case SG_VERTEXFORMAT_UINT3:
        return { components: 3, gl_type: gl.UNSIGNED_INT, normalized: false };
      case SG_VERTEXFORMAT_UINT4:
        return { components: 4, gl_type: gl.UNSIGNED_INT, normalized: false };
      case SG_VERTEXFORMAT_BYTE4:
        return { components: 4, gl_type: gl.BYTE, normalized: false };
      case SG_VERTEXFORMAT_BYTE4N:
        return { components: 4, gl_type: gl.BYTE, normalized: true };
      case SG_VERTEXFORMAT_UBYTE4:
        return { components: 4, gl_type: gl.UNSIGNED_BYTE, normalized: false };
      case SG_VERTEXFORMAT_UBYTE4N:
        return { components: 4, gl_type: gl.UNSIGNED_BYTE, normalized: true };
      case SG_VERTEXFORMAT_SHORT2:
        return { components: 2, gl_type: gl.SHORT, normalized: false };
      case SG_VERTEXFORMAT_SHORT2N:
        return { components: 2, gl_type: gl.SHORT, normalized: true };
      case SG_VERTEXFORMAT_USHORT2:
        return { components: 2, gl_type: gl.UNSIGNED_SHORT, normalized: false };
      case SG_VERTEXFORMAT_USHORT2N:
        return { components: 2, gl_type: gl.UNSIGNED_SHORT, normalized: true };
      case SG_VERTEXFORMAT_SHORT4:
        return { components: 4, gl_type: gl.SHORT, normalized: false };
      case SG_VERTEXFORMAT_SHORT4N:
        return { components: 4, gl_type: gl.SHORT, normalized: true };
      case SG_VERTEXFORMAT_USHORT4:
        return { components: 4, gl_type: gl.UNSIGNED_SHORT, normalized: false };
      case SG_VERTEXFORMAT_USHORT4N:
        return { components: 4, gl_type: gl.UNSIGNED_SHORT, normalized: true };
      case SG_VERTEXFORMAT_HALF2:
        return { components: 2, gl_type: gl.HALF_FLOAT, normalized: false };
      case SG_VERTEXFORMAT_HALF4:
        return { components: 4, gl_type: gl.HALF_FLOAT, normalized: false };
      default:
        console.error("Unknown vertex format:", format);
        return { components: 4, gl_type: gl.FLOAT, normalized: false };
    }
  }

  function js_webgl_create_image(
    id: number,
    gl_target: number,
    width: number,
    height: number,
    gl_internal_format: number,
    gl_format: number,
    gl_type: number,
    is_render_target: boolean,
    is_compressed: boolean,
    num_mipmaps: number,
    mipmap_data_ptrs_ptr: number,
    mipmap_data_sizes_ptr: number,
  ): number {
    const texture = gl.createTexture();
    if (!texture) {
      console.error("Failed to create WebGL texture");
      return 0;
    }

    gl.bindTexture(gl_target, texture);

    const SG_MAX_MIPMAPS = 16;
    const num_faces = gl_target === gl.TEXTURE_CUBE_MAP ? 6 : 1;

    if (is_render_target) {
      gl.texStorage2D(gl_target, 1, gl_internal_format, width, height);
      gl.texParameteri(gl_target, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl_target, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    } else if (gl_target === gl.TEXTURE_CUBE_MAP) {
      for (let face = 0; face < 6; face++) {
        const faceTarget = gl.TEXTURE_CUBE_MAP_POSITIVE_X + face;
        let mip_width = width;
        let mip_height = height;

        for (let mip = 0; mip < num_mipmaps; mip++) {
          const idx = face * SG_MAX_MIPMAPS + mip;
          const data_ptr = wasmMemory.loadU32(mipmap_data_ptrs_ptr + idx * 4);
          const data_size = wasmMemory.loadU32(mipmap_data_sizes_ptr + idx * 4);

          if (data_ptr !== 0 && data_size > 0) {
            const data = wasmMemory.loadU8Array(data_ptr, data_size);
            if (is_compressed) {
              gl.compressedTexImage2D(
                faceTarget,
                mip,
                gl_internal_format,
                mip_width,
                mip_height,
                0,
                data,
              );
            } else {
              gl.texImage2D(
                faceTarget,
                mip,
                gl_internal_format,
                mip_width,
                mip_height,
                0,
                gl_format,
                gl_type,
                data,
              );
            }
          }

          mip_width = Math.max(1, mip_width >> 1);
          mip_height = Math.max(1, mip_height >> 1);
        }
      }

      gl.texParameteri(
        gl_target,
        gl.TEXTURE_MIN_FILTER,
        num_mipmaps > 1 ? gl.LINEAR_MIPMAP_LINEAR : gl.LINEAR,
      );
      gl.texParameteri(gl_target, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
    } else {
      if (
        num_mipmaps === 0 ||
        (mipmap_data_ptrs_ptr === 0 && mipmap_data_sizes_ptr === 0)
      ) {
        gl.texStorage2D(gl_target, 1, gl_internal_format, width, height);
        gl.texParameteri(gl_target, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      } else {
        gl.texStorage2D(
          gl_target,
          num_mipmaps,
          gl_internal_format,
          width,
          height,
        );

        let mip_width = width;
        let mip_height = height;

        for (let mip = 0; mip < num_mipmaps; mip++) {
          const idx = mip;
          const data_ptr = wasmMemory.loadU32(mipmap_data_ptrs_ptr + idx * 4);
          const data_size = wasmMemory.loadU32(mipmap_data_sizes_ptr + idx * 4);

          if (data_ptr !== 0 && data_size > 0) {
            const data = wasmMemory.loadU8Array(data_ptr, data_size);
            if (is_compressed) {
              gl.compressedTexSubImage2D(
                gl_target,
                mip,
                0,
                0,
                mip_width,
                mip_height,
                gl_internal_format,
                data,
              );
            } else {
              gl.texSubImage2D(
                gl_target,
                mip,
                0,
                0,
                mip_width,
                mip_height,
                gl_format,
                gl_type,
                data,
              );
            }
          }

          mip_width = Math.max(1, mip_width >> 1);
          mip_height = Math.max(1, mip_height >> 1);
        }

        gl.texParameteri(
          gl_target,
          gl.TEXTURE_MIN_FILTER,
          num_mipmaps > 1 ? gl.LINEAR_MIPMAP_LINEAR : gl.LINEAR,
        );
      }
      gl.texParameteri(gl_target, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl_target, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    }

    gl.bindTexture(gl_target, null);
    renderer.textures[id] = { texture, gl_target };
    return id;
  }

  function js_webgl_create_buffer(
    id: number,
    size: number,
    is_vertex_buffer: boolean,
    is_index_buffer: boolean,
    data_ptr: number,
    data_size: number,
  ): number {
    const buffer = gl.createBuffer();
    if (!buffer) {
      console.error("Failed to create WebGL buffer");
      return 0;
    }

    const target = is_index_buffer ? gl.ELEMENT_ARRAY_BUFFER : gl.ARRAY_BUFFER;

    gl.bindBuffer(target, buffer);

    //todo: where are the options??
    //todo: support for storage buffer?
    if (data_ptr !== 0 && data_size > 0) {
      const data = wasmMemory.loadU8Array(data_ptr, data_size);
      gl.bufferData(target, data, gl.STATIC_DRAW);
    } else {
      gl.bufferData(target, size, gl.STATIC_DRAW);
    }

    gl.bindBuffer(target, null);
    renderer.buffers[id] = buffer;
    return id;
  }

  function js_webgl_create_sampler(
    id: number,
    gl_min_filter: number,
    gl_mag_filter: number,
    gl_wrap_s: number,
    gl_wrap_t: number,
    gl_wrap_r: number,
  ): number {
    const sampler = gl.createSampler();
    if (!sampler) {
      console.error("Failed to create WebGL sampler");
      return 0;
    }

    console.log(gl_min_filter, gl_mag_filter);
    gl.samplerParameteri(sampler, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.samplerParameteri(sampler, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.samplerParameteri(sampler, gl.TEXTURE_WRAP_S, gl_wrap_s);
    gl.samplerParameteri(sampler, gl.TEXTURE_WRAP_T, gl_wrap_t);
    gl.samplerParameteri(sampler, gl.TEXTURE_WRAP_R, gl_wrap_r);

    renderer.samplers[id] = sampler;
    return id;
  }

  function js_webgl_create_shader(
    id: number,
    vs_source_ptr: number,
    vs_source_len: number,
    fs_source_ptr: number,
    fs_source_len: number,
    uniform_names_ptr: number,
    uniform_name_lens_ptr: number,
    uniform_slots_ptr: number,
    num_uniforms: number,
    sampler_names_ptr: number,
    sampler_name_lens_ptr: number,
    sampler_tex_units_ptr: number,
    num_samplers: number,
  ): number {
    const vs_source = new TextDecoder().decode(
      wasmMemory.loadU8Array(vs_source_ptr, vs_source_len),
    );
    const fs_source = new TextDecoder().decode(
      wasmMemory.loadU8Array(fs_source_ptr, fs_source_len),
    );

    const vs = gl.createShader(gl.VERTEX_SHADER);
    if (!vs) {
      console.error("Failed to create vertex shader");
      return 0;
    }

    gl.shaderSource(vs, vs_source);
    gl.compileShader(vs);

    if (!gl.getShaderParameter(vs, gl.COMPILE_STATUS)) {
      const info = gl.getShaderInfoLog(vs);
      console.error("Vertex shader compilation failed:", info);
      console.error("Vertex shader source:", vs_source);
      gl.deleteShader(vs);
      return 0;
    }

    const fs = gl.createShader(gl.FRAGMENT_SHADER);
    if (!fs) {
      console.error("Failed to create fragment shader");
      gl.deleteShader(vs);
      return 0;
    }

    gl.shaderSource(fs, fs_source);
    gl.compileShader(fs);

    if (!gl.getShaderParameter(fs, gl.COMPILE_STATUS)) {
      const info = gl.getShaderInfoLog(fs);
      console.error("Fragment shader compilation failed:", info);
      console.error("Fragment shader source:", fs_source);
      gl.deleteShader(vs);
      gl.deleteShader(fs);
      return 0;
    }

    const program = gl.createProgram();
    if (!program) {
      console.error("Failed to create shader program");
      gl.deleteShader(vs);
      gl.deleteShader(fs);
      return 0;
    }

    gl.attachShader(program, vs);
    gl.attachShader(program, fs);
    gl.linkProgram(program);

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      const info = gl.getProgramInfoLog(program);
      console.error("Shader program linking failed:", info);
      gl.deleteProgram(program);
      gl.deleteShader(vs);
      gl.deleteShader(fs);
      return 0;
    }

    gl.deleteShader(vs);
    gl.deleteShader(fs);

    gl.useProgram(program);

    for (let i = 0; i < num_samplers; i++) {
      const name_ptr_offset = sampler_names_ptr + i * 4;
      const name_ptr = wasmMemory.loadU32(name_ptr_offset);
      const name_len = wasmMemory.loadU32(sampler_name_lens_ptr + i * 4);
      const tex_unit = wasmMemory.loadU32(sampler_tex_units_ptr + i * 4);

      const name = new TextDecoder().decode(
        wasmMemory.loadU8Array(name_ptr, name_len),
      );

      const location = gl.getUniformLocation(program, name);
      if (location !== null) {
        gl.uniform1i(location, tex_unit);
      }
    }

    //cache uniforms
    const uniform_locations: (WebGLUniformLocation | null)[][] = [];
    for (let s = 0; s < 8; s++) {
      uniform_locations[s] = [];
    }

    for (let i = 0; i < num_uniforms; i++) {
      const name_ptr_offset = uniform_names_ptr + i * 4;
      const name_ptr = wasmMemory.loadU32(name_ptr_offset);
      const name_len = wasmMemory.loadU32(uniform_name_lens_ptr + i * 4);
      const slot = wasmMemory.loadU32(uniform_slots_ptr + i * 4);

      const name = new TextDecoder().decode(
        wasmMemory.loadU8Array(name_ptr, name_len),
      );

      const location = gl.getUniformLocation(program, name);
      uniform_locations[slot].push(location);
    }

    renderer.shaders[id] = { program, uniform_locations };
    return id;
  }

  function js_webgl_begin_pass_swapchain(
    r: number,
    g: number,
    b: number,
    a: number,
    clear_color: boolean,
    clear_depth: boolean,
  ) {
    //todo: need to nuke string matching
    renderer.current_pass_target = "SWAPCHAIN";
    renderer.draw_call_count = 0;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, canvas.width, canvas.height);

    let clear_bits = 0;
    if (clear_color) {
      gl.clearColor(r, g, b, a);
      clear_bits |= gl.COLOR_BUFFER_BIT;
    }
    if (clear_depth) {
      gl.depthMask(true);
      clear_bits |= gl.DEPTH_BUFFER_BIT;
    }
    if (clear_bits !== 0) {
      gl.clear(clear_bits);
    }
  }

  function js_webgl_create_framebuffer(
    color_img_id: number,
    depth_img_id: number,
  ): number {
    const fbo = gl.createFramebuffer();
    if (!fbo) {
      throw new Error("Failed to create framebuffer");
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);

    if (color_img_id !== 0) {
      const color_texture_data = renderer.textures[color_img_id];
      if (color_texture_data) {
        gl.framebufferTexture2D(
          gl.FRAMEBUFFER,
          gl.COLOR_ATTACHMENT0,
          gl.TEXTURE_2D,
          color_texture_data.texture,
          0,
        );
        console.log(
          `[FBO CACHE] Attached color texture ${color_img_id} to FBO`,
        );
      } else {
        console.error("Color texture not found for id:", color_img_id);
      }
    }

    if (depth_img_id !== 0) {
      const depth_texture_data = renderer.textures[depth_img_id];
      if (depth_texture_data) {
        gl.framebufferTexture2D(
          gl.FRAMEBUFFER,
          gl.DEPTH_ATTACHMENT,
          gl.TEXTURE_2D,
          depth_texture_data.texture,
          0,
        );
        console.log(
          `[FBO CACHE] Attached depth texture ${depth_img_id} to FBO`,
        );
      } else {
        console.error("Depth texture not found for id:", depth_img_id);
      }
    }

    const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (status !== gl.FRAMEBUFFER_COMPLETE) {
      console.error("Framebuffer incomplete:", status);
    }

    const fbo_id = renderer.framebuffers.length;
    renderer.framebuffers.push(fbo);
    console.log(
      `[FBO CACHE] Created FBO with id=${fbo_id} (PERMANENT attachments)`,
    );
    return fbo_id;
  }

  function js_webgl_begin_pass_framebuffer(
    fbo_id: number,
    r: number,
    g: number,
    b: number,
    a: number,
    clear_color: boolean,
    clear_depth: boolean,
  ) {
    const fbo = renderer.framebuffers[fbo_id];
    if (!fbo) {
      console.error("Framebuffer not found for id:", fbo_id);
      return;
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
    gl.viewport(0, 0, canvas.width, canvas.height);

    let clear_bits = 0;
    if (clear_color) {
      gl.clearColor(r, g, b, a);
      clear_bits |= gl.COLOR_BUFFER_BIT;
    }
    if (clear_depth) {
      gl.depthMask(true);
      clear_bits |= gl.DEPTH_BUFFER_BIT;
    }
    if (clear_bits !== 0) {
      gl.clear(clear_bits);
    }
  }

  function js_webgl_apply_pipeline(
    shader_id: number,
    gl_depth_func: number,
    depth_write_enabled: boolean,
    gl_cull_mode: number,
    gl_front_face: number,
    blend_enabled: boolean,
    gl_blend_src_rgb: number,
    gl_blend_dst_rgb: number,
    gl_blend_op_rgb: number,
    gl_blend_src_alpha: number,
    gl_blend_dst_alpha: number,
    gl_blend_op_alpha: number,
  ) {
    renderer.current_shader_id = shader_id;
    const shader = renderer.shaders[shader_id];
    if (!shader) {
      return;
    }

    gl.useProgram(shader.program);

    if (gl_depth_func !== gl.ALWAYS) {
      gl.enable(gl.DEPTH_TEST);
      gl.depthFunc(gl_depth_func);
    } else {
      gl.disable(gl.DEPTH_TEST);
    }

    gl.depthMask(depth_write_enabled);

    if (gl_cull_mode === gl.NONE) {
      gl.disable(gl.CULL_FACE);
    } else {
      gl.enable(gl.CULL_FACE);
      gl.cullFace(gl_cull_mode);
    }

    gl.frontFace(gl_front_face);

    if (blend_enabled) {
      gl.enable(gl.BLEND);
      gl.blendFuncSeparate(
        gl_blend_src_rgb,
        gl_blend_dst_rgb,
        gl_blend_src_alpha,
        gl_blend_dst_alpha,
      );
      gl.blendEquationSeparate(gl_blend_op_rgb, gl_blend_op_alpha);
    } else {
      gl.disable(gl.BLEND);
    }
  }

  function js_webgl_apply_bindings(
    vb_ids_ptr: number,
    num_vbs: number,
    ib_id: number,
    texture_ids_ptr: number,
    num_textures: number,
    sampler_ids_ptr: number,
    num_samplers: number,
    attr_formats_ptr: number,
    attr_offsets_ptr: number,
    attr_buffer_indices_ptr: number,
    buffer_strides_ptr: number,
    buffer_step_funcs_ptr: number,
    buffer_step_rates_ptr: number,
  ) {
    const vb_ids =
      num_vbs > 0
        ? wasmMemory.loadU32Array(vb_ids_ptr, num_vbs)
        : new Uint32Array(0);
    const ib_idx = ib_id;
    renderer.current_vb_id = vb_ids.length > 0 ? vb_ids[0] : -1;
    renderer.current_ib_id = ib_id;

    const ib = renderer.buffers[ib_idx];

    if (num_textures > 0 && texture_ids_ptr !== 0) {
      const texture_ids = wasmMemory.loadU32Array(
        texture_ids_ptr,
        num_textures,
      );
      const sampler_ids =
        num_samplers > 0 && sampler_ids_ptr !== 0
          ? wasmMemory.loadU32Array(sampler_ids_ptr, num_samplers)
          : null;

      for (let i = 0; i < num_textures; i++) {
        const texture_id = texture_ids[i];
        const texture_data = renderer.textures[texture_id];

        if (texture_data) {
          gl.activeTexture(gl.TEXTURE0 + i);
          gl.bindTexture(texture_data.gl_target, texture_data.texture);

          if (sampler_ids && i < num_samplers) {
            const sampler_id = sampler_ids[i];
            const sampler = renderer.samplers[sampler_id];
            if (sampler) {
              gl.bindSampler(i, sampler);
            }
          }
        }
      }
    }

    const attr_formats = wasmMemory.loadU32Array(attr_formats_ptr, 16);
    const attr_offsets = wasmMemory.loadI32Array(attr_offsets_ptr, 16);
    const attr_buffer_indices = wasmMemory.loadI32Array(
      attr_buffer_indices_ptr,
      16,
    );
    const buffer_strides = wasmMemory.loadI32Array(buffer_strides_ptr, 8);
    const buffer_step_funcs = wasmMemory.loadU32Array(buffer_step_funcs_ptr, 8);
    const buffer_step_rates = wasmMemory.loadI32Array(buffer_step_rates_ptr, 8);

    const SG_VERTEXSTEP_PER_VERTEX = 1;
    const SG_VERTEXSTEP_PER_INSTANCE = 2;

    for (let i = 0; i < 16; i++) {
      const format = attr_formats[i];
      if (format === 0) {
        gl.disableVertexAttribArray(i);
        continue;
      }

      const buffer_index = attr_buffer_indices[i];
      const vb_id = vb_ids[buffer_index];
      const buffer = renderer.buffers[vb_id];

      if (!buffer) {
        console.warn(
          `[BINDINGS] Attribute ${i} references missing buffer index ${buffer_index} (vb_id=${vb_id})`,
        );
        gl.disableVertexAttribArray(i);
        continue;
      }

      const stride = buffer_strides[buffer_index];
      const offset = attr_offsets[i];
      const { components, gl_type, normalized } = vertexFormatToGL(format);

      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.enableVertexAttribArray(i);

      const isInteger =
        gl_type === gl.INT ||
        gl_type === gl.UNSIGNED_INT ||
        (gl_type === gl.BYTE && !normalized) ||
        (gl_type === gl.UNSIGNED_BYTE && !normalized) ||
        (gl_type === gl.SHORT && !normalized) ||
        (gl_type === gl.UNSIGNED_SHORT && !normalized);

      if (isInteger) {
        gl.vertexAttribIPointer(i, components, gl_type, stride, offset);
      } else {
        gl.vertexAttribPointer(
          i,
          components,
          gl_type,
          normalized,
          stride,
          offset,
        );
      }

      const step_func = buffer_step_funcs[buffer_index];
      if (step_func === SG_VERTEXSTEP_PER_INSTANCE) {
        const step_rate = buffer_step_rates[buffer_index];
        gl.vertexAttribDivisor(i, step_rate);
      } else {
        gl.vertexAttribDivisor(i, 0);
      }
    }

    if (ib) {
      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ib);
    }
  }

  function js_webgl_apply_uniforms(
    shader_id: number,
    slot: number,
    types_ptr: number,
    counts_ptr: number,
    offsets_ptr: number,
    num_uniforms: number,
    data_ptr: number,
    data_size: number,
  ) {
    const shader = renderer.shaders[shader_id];
    if (!shader) {
      return;
    }

    const locations = shader.uniform_locations[slot];
    if (!locations) {
      return;
    }

    const types = wasmMemory.loadU32Array(types_ptr, num_uniforms);
    const counts = wasmMemory.loadU16Array(counts_ptr, num_uniforms);
    const offsets = wasmMemory.loadU16Array(offsets_ptr, num_uniforms);

    //todo: need sync with C types, better way to do this?
    const SG_UNIFORMTYPE_FLOAT = 1;
    const SG_UNIFORMTYPE_FLOAT2 = 2;
    const SG_UNIFORMTYPE_FLOAT3 = 3;
    const SG_UNIFORMTYPE_FLOAT4 = 4;
    const SG_UNIFORMTYPE_INT = 5;
    const SG_UNIFORMTYPE_INT2 = 6;
    const SG_UNIFORMTYPE_INT3 = 7;
    const SG_UNIFORMTYPE_INT4 = 8;
    const SG_UNIFORMTYPE_MAT4 = 9;

    for (let i = 0; i < num_uniforms; i++) {
      const location = locations[i];
      if (!location) {
        continue;
      }

      const type = types[i];
      const array_count = counts[i] > 0 ? counts[i] : 1;
      const offset = offsets[i];

      switch (type) {
        case SG_UNIFORMTYPE_FLOAT: {
          const data = wasmMemory.loadF32Array(data_ptr + offset, array_count);
          if (array_count === 1) {
            gl.uniform1f(location, data[0]);
          } else {
            gl.uniform1fv(location, data);
          }
          break;
        }
        case SG_UNIFORMTYPE_FLOAT2: {
          const data = wasmMemory.loadF32Array(
            data_ptr + offset,
            2 * array_count,
          );
          gl.uniform2fv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_FLOAT3: {
          const data = wasmMemory.loadF32Array(
            data_ptr + offset,
            3 * array_count,
          );
          gl.uniform3fv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_FLOAT4: {
          const data = wasmMemory.loadF32Array(
            data_ptr + offset,
            4 * array_count,
          );
          gl.uniform4fv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_INT: {
          const data = wasmMemory.loadI32Array(data_ptr + offset, array_count);
          if (array_count === 1) {
            gl.uniform1i(location, data[0]);
          } else {
            gl.uniform1iv(location, data);
          }
          break;
        }
        case SG_UNIFORMTYPE_INT2: {
          const data = wasmMemory.loadI32Array(
            wasmMemory.memory.buffer,
            data_ptr + offset,
            2 * array_count,
          );
          gl.uniform2iv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_INT3: {
          const data = wasmMemory.loadI32Array(
            wasmMemory.memory.buffer,
            data_ptr + offset,
            3 * array_count,
          );
          gl.uniform3iv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_INT4: {
          const data = wasmMemory.loadI32Array(
            wasmMemory.memory.buffer,
            data_ptr + offset,
            4 * array_count,
          );
          gl.uniform4iv(location, data);
          break;
        }
        case SG_UNIFORMTYPE_MAT4: {
          gl.mat;
          const data = wasmMemory.loadF32Array(
            data_ptr + offset,
            16 * array_count,
          );
          gl.uniformMatrix4fv(location, false, data);
          break;
        }
        default:
          console.error("Unknown uniform type:", type);
          break;
      }
    }
  }

  function js_webgl_draw(
    base_element: number,
    num_elements: number,
    num_instances: number,
    gl_primitive_type: number,
  ) {
    const has_index_buffer = renderer.current_ib_id >= 0;

    renderer.draw_call_counter++;
    const current_fbo = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    const is_swapchain = current_fbo === null;

    if (num_instances > 1) {
      if (has_index_buffer) {
        gl.drawElementsInstanced(
          gl_primitive_type,
          num_elements,
          gl.UNSIGNED_INT,
          base_element * 4,
          num_instances,
        );
      } else {
        gl.drawArraysInstanced(
          gl_primitive_type,
          base_element,
          num_elements,
          num_instances,
        );
      }
    } else {
      if (has_index_buffer) {
        gl.drawElements(
          gl_primitive_type,
          num_elements,
          gl.UNSIGNED_INT,
          base_element * 4,
        );
      } else {
        gl.drawArrays(gl_primitive_type, base_element, num_elements);
      }
    }

    // NOTE: DO NOT ENABLE THIS IF NOT DEBUGGING. DESTROYS PERFORMANCE
    // const err = gl.getError();
    // if (err !== gl.NO_ERROR) {
    //   console.error("[DRAW] WebGL Error:", err);
    // }
  }

  function sapp_width(): number {
    return canvas.width;
  }

  function sapp_height(): number {
    return canvas.height;
  }

  function sapp_dpi_scale(): number {
    return window.devicePixelRatio || 1.0;
  }

  function js_webgl_apply_scissor_rect(
    x: number,
    y: number,
    width: number,
    height: number,
    origin_top_left: boolean,
  ) {
    const canvas_height = canvas.height;
    const scissor_y = origin_top_left ? y : canvas_height - (y + height);

    gl.enable(gl.SCISSOR_TEST);
    gl.scissor(x, scissor_y, width, height);
  }

  function js_webgl_destroy_buffer(id: number) {
    const buffer = renderer.buffers[id];
    if (buffer) {
      gl.deleteBuffer(buffer);
      renderer.buffers[id] = null;
    } else {
      console.warn(`[DESTROY_BUFFER] id=${id} - buffer not found`);
    }
  }

  function js_webgl_destroy_image(id: number) {
    const texture_data = renderer.textures[id];
    if (texture_data) {
      gl.deleteTexture(texture_data.texture);
      renderer.textures[id] = null;
    }
  }

  function js_webgl_destroy_shader(id: number) {
    const shader = renderer.shaders[id];
    if (shader) {
      gl.deleteProgram(shader.program);
      renderer.shaders[id] = null;
    }
  }

  function js_webgl_destroy_sampler(id: number) {
    const sampler = renderer.samplers[id];
    if (sampler) {
      gl.deleteSampler(sampler);
      renderer.samplers[id] = null;
    }
  }

  function js_webgl_update_buffer(
    id: number,
    gl_target: number,
    data_ptr: number,
    data_size: number,
  ) {
    const buffer = renderer.buffers[id];
    if (!buffer) {
      console.error(`[UPDATE_BUFFER] Buffer id=${id} not found`);
      return;
    }

    const data = wasmMemory.loadU8Array(data_ptr, data_size);

    gl.bindBuffer(gl_target, buffer);
    gl.bufferSubData(gl_target, 0, data);
    gl.bindBuffer(gl_target, null);
  }

  return {
    gl,
    js_webgl_create_image,
    js_webgl_create_buffer,
    js_webgl_create_sampler,
    js_webgl_create_shader,
    js_webgl_begin_pass_swapchain,
    js_webgl_create_framebuffer,
    js_webgl_begin_pass_framebuffer,
    js_webgl_apply_pipeline,
    js_webgl_apply_bindings,
    js_webgl_apply_uniforms,
    js_webgl_draw,
    sapp_width,
    sapp_height,
    sapp_dpi_scale,
    js_webgl_apply_scissor_rect,
    js_webgl_update_buffer,
    js_webgl_destroy_buffer,
    js_webgl_destroy_image,
    js_webgl_destroy_shader,
    js_webgl_destroy_sampler,
    _os_get_compressed_texture_format,
  };
}
