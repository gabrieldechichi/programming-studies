class HandleArray {
  constructor() {
    this.items = [];
    this.freelist = [];
    this.num = 0;
  }

  add(v) {
    if (this.freelist.length > 0) {
      const idx = this.freelist.pop();
      this.items[idx] = v;
      this.num += 1;
      return idx;
    }

    if (this.items.length === 0) {
      this.items.push(null);
    }

    const idx = this.items.length;
    this.items.push(v);
    this.num += 1;
    return idx;
  }

  get(idx) {
    if (idx >= 0 && idx < this.items.length) {
      return this.items[idx];
    }
    return null;
  }

  remove(idx) {
    if (idx >= 0 && idx < this.items.length && this.items[idx] !== null) {
      this.items[idx] = null;
      this.freelist.push(idx);
      this.num -= 1;
    }
  }
}

const WebGLIndexType = {
  NONE: 0,
  UINT32: 1,
};

const WebGLPrimitiveType = {
  TRIANGLES: 0,
  TRIANGLE_STRIP: 1,
};

const WebGLCullMode = {
  NONE: 0,
  BACK: 1,
  FRONT: 2,
};

const WebGLCompareFunc = {
  ALWAYS: 0,
  LESS_EQUAL: 1,
};

const WebGLFilter = {
  LINEAR: 0,
  NEAREST: 1,
};

const WebGLWrap = {
  REPEAT: 0,
  CLAMP_TO_BORDER: 1,
};

const WebGLPixelFormat = {
  RGBA8: 0,
};

export function createWebGLRenderer(wasmMemory, canvas) {
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

  const buffers = new HandleArray();
  const textures = new HandleArray();
  const programs = new HandleArray();
  const pipelines = new HandleArray();
  const samplers = new HandleArray();

  let currentPipeline = null;
  const currentBindings = {
    vertexBuffers: {},
    indexBuffer: null,
    textures: {},
    samplers: {},
    storageBuffers: {},
  };

  function _webgl_make_buffer(
    data_ptr,
    data_size,
    usage_vertex,
    usage_index,
    usage_storage,
    immutable
  ) {
    const buffer = gl.createBuffer();
    if (!buffer) {
      console.error("Failed to create WebGL buffer");
      return 0;
    }

    let target = null;
    if (usage_vertex) {
      target = gl.ARRAY_BUFFER;
    } else if (usage_index) {
      target = gl.ELEMENT_ARRAY_BUFFER;
    } else if (usage_storage) {
      target = gl.ARRAY_BUFFER;
    }

    if (!target) {
      console.error("Invalid buffer usage flags");
      gl.deleteBuffer(buffer);
      return 0;
    }

    gl.bindBuffer(target, buffer);

    const data = new Uint8Array(wasmMemory.memory.buffer, data_ptr, data_size);
    const usage = immutable ? gl.STATIC_DRAW : gl.DYNAMIC_DRAW;
    gl.bufferData(target, data, usage);

    gl.bindBuffer(target, null);

    const bufferObj = {
      buffer,
      target,
      size: data_size,
    };

    return buffers.add(bufferObj);
  }

  function _webgl_destroy_buffer(id) {
    const bufferObj = buffers.get(id);
    if (bufferObj) {
      gl.deleteBuffer(bufferObj.buffer);
      buffers.remove(id);
    }
  }

  function _webgl_make_image(
    width,
    height,
    pixel_format,
    data_ptr,
    data_size
  ) {
    const texture = gl.createTexture();
    if (!texture) {
      console.error("Failed to create WebGL texture");
      return 0;
    }

    gl.bindTexture(gl.TEXTURE_2D, texture);

    let format = gl.RGBA;
    let internalFormat = gl.RGBA8;
    let type = gl.UNSIGNED_BYTE;

    if (pixel_format === WebGLPixelFormat.RGBA8) {
      format = gl.RGBA;
      internalFormat = gl.RGBA8;
      type = gl.UNSIGNED_BYTE;
    }

    const data = new Uint8Array(wasmMemory.memory.buffer, data_ptr, data_size);

    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      internalFormat,
      width,
      height,
      0,
      format,
      type,
      data
    );

    gl.bindTexture(gl.TEXTURE_2D, null);

    const textureObj = {
      texture,
      width,
      height,
    };

    return textures.add(textureObj);
  }

  function _webgl_make_texture_view(image_id) {
    return image_id;
  }

  function _webgl_destroy_image(id) {
    const textureObj = textures.get(id);
    if (textureObj) {
      gl.deleteTexture(textureObj.texture);
      textures.remove(id);
    }
  }

  function _webgl_make_shader(vs_src_ptr, fs_src_ptr, vs_len, fs_len) {
    const vs_src = wasmMemory.loadString(vs_src_ptr, vs_len);
    const fs_src = wasmMemory.loadString(fs_src_ptr, fs_len);

    const vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, vs_src);
    gl.compileShader(vertexShader);

    if (!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
      console.error(
        "Vertex shader compilation failed:",
        gl.getShaderInfoLog(vertexShader)
      );
      gl.deleteShader(vertexShader);
      return 0;
    }

    const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, fs_src);
    gl.compileShader(fragmentShader);

    if (!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS)) {
      console.error(
        "Fragment shader compilation failed:",
        gl.getShaderInfoLog(fragmentShader)
      );
      gl.deleteShader(vertexShader);
      gl.deleteShader(fragmentShader);
      return 0;
    }

    const program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      console.error(
        "Shader program linking failed:",
        gl.getProgramInfoLog(program)
      );
      gl.deleteShader(vertexShader);
      gl.deleteShader(fragmentShader);
      gl.deleteProgram(program);
      return 0;
    }

    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    const uniformLocations = {};
    const numUniforms = gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS);
    for (let i = 0; i < numUniforms; i++) {
      const info = gl.getActiveUniform(program, i);
      const location = gl.getUniformLocation(program, info.name);
      uniformLocations[info.name] = location;
    }

    const programObj = {
      program,
      uniformLocations,
    };

    return programs.add(programObj);
  }

  function _webgl_destroy_shader(id) {
    const programObj = programs.get(id);
    if (programObj) {
      gl.deleteProgram(programObj.program);
      programs.remove(id);
    }
  }

  function _webgl_make_pipeline(
    shader_id,
    index_type,
    primitive_type,
    cull_mode,
    depth_write,
    depth_compare
  ) {
    const programObj = programs.get(shader_id);
    if (!programObj) {
      console.error("Invalid shader ID for pipeline");
      return 0;
    }

    gl.useProgram(programObj.program);

    // For GLSL300ES, sokol-shdc generates uniform arrays instead of UBOs
    // We need to track the uniform locations for these arrays
    const uniformArrayLocations = {};

    // Map slot numbers to uniform array names
    const slotToUniformName = {
      0: 'camera_params',
      1: 'joint_transforms',
      2: 'model_params',
      3: 'fs_params',
      4: 'light_params',
      6: 'blendshape_params'
    };

    // Get locations for each uniform array
    Object.entries(slotToUniformName).forEach(([slot, name]) => {
      const location = gl.getUniformLocation(programObj.program, name);
      if (location !== null) {
        uniformArrayLocations[parseInt(slot)] = location;
      }
    });

    const pipelineObj = {
      program: programObj.program,
      uniformLocations: programObj.uniformLocations,
      uniformArrayLocations: uniformArrayLocations,
      indexType:
        index_type === WebGLIndexType.UINT32
          ? gl.UNSIGNED_INT
          : gl.UNSIGNED_SHORT,
      primitiveType:
        primitive_type === WebGLPrimitiveType.TRIANGLES
          ? gl.TRIANGLES
          : gl.TRIANGLE_STRIP,
      cullMode: cull_mode,
      depthWrite: depth_write !== 0,
      depthCompare: depth_compare,
    };

    return pipelines.add(pipelineObj);
  }

  function _webgl_destroy_pipeline(id) {
    pipelines.remove(id);
  }

  function _webgl_make_sampler(min_filter, mag_filter, wrap_u, wrap_v) {
    const sampler = gl.createSampler();
    if (!sampler) {
      console.error("Failed to create sampler");
      return 0;
    }

    const minFilter =
      min_filter === WebGLFilter.LINEAR ? gl.LINEAR : gl.NEAREST;
    const magFilter =
      mag_filter === WebGLFilter.LINEAR ? gl.LINEAR : gl.NEAREST;
    const wrapS = wrap_u === WebGLWrap.REPEAT ? gl.REPEAT : gl.CLAMP_TO_EDGE;
    const wrapT = wrap_v === WebGLWrap.REPEAT ? gl.REPEAT : gl.CLAMP_TO_EDGE;

    gl.samplerParameteri(sampler, gl.TEXTURE_MIN_FILTER, minFilter);
    gl.samplerParameteri(sampler, gl.TEXTURE_MAG_FILTER, magFilter);
    gl.samplerParameteri(sampler, gl.TEXTURE_WRAP_S, wrapS);
    gl.samplerParameteri(sampler, gl.TEXTURE_WRAP_T, wrapT);

    const samplerObj = { sampler };
    return samplers.add(samplerObj);
  }

  function _webgl_begin_pass(
    clear_r,
    clear_g,
    clear_b,
    clear_a,
    clear_color,
    clear_depth
  ) {
    gl.viewport(0, 0, canvas.width, canvas.height);

    let clearMask = 0;

    if (clear_color) {
      gl.clearColor(clear_r, clear_g, clear_b, clear_a);
      clearMask |= gl.COLOR_BUFFER_BIT;
    }

    if (clear_depth) {
      gl.clearDepth(1.0);
      clearMask |= gl.DEPTH_BUFFER_BIT;
    }

    if (clearMask !== 0) {
      gl.clear(clearMask);
    }

    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
  }

  function _webgl_end_pass() {}

  function _webgl_apply_pipeline(pipeline_id) {
    const pipeline = pipelines.get(pipeline_id);
    if (!pipeline) {
      console.error("Invalid pipeline ID");
      return;
    }

    currentPipeline = pipeline;

    gl.useProgram(pipeline.program);

    if (pipeline.cullMode === WebGLCullMode.BACK) {
      gl.enable(gl.CULL_FACE);
      gl.cullFace(gl.BACK);
    } else if (pipeline.cullMode === WebGLCullMode.FRONT) {
      gl.enable(gl.CULL_FACE);
      gl.cullFace(gl.FRONT);
    } else {
      gl.disable(gl.CULL_FACE);
    }

    gl.depthMask(pipeline.depthWrite);

    if (pipeline.depthCompare === WebGLCompareFunc.ALWAYS) {
      gl.depthFunc(gl.ALWAYS);
    } else {
      gl.depthFunc(gl.LEQUAL);
    }
  }

  function _webgl_apply_vertex_buffer(slot, buffer_id) {
    const bufferObj = buffers.get(buffer_id);
    if (!bufferObj) {
      console.error("Invalid buffer ID for vertex buffer");
      return;
    }

    currentBindings.vertexBuffers[slot] = bufferObj;

    gl.bindBuffer(gl.ARRAY_BUFFER, bufferObj.buffer);

    if (slot === 0) {
      // Check if this is a skinned mesh by looking at the stride
      // Static mesh: position(3f) + normal(3f) + texcoord(2f) = 32 bytes
      // Skinned mesh: position(3f) + normal(3f) + texcoord(2f) + joints(4ub) + weights(4f) = 52 bytes
      const isSkinnedMesh = bufferObj.size % 52 === 0;
      const stride = isSkinnedMesh ? 52 : 32;

      // Position attribute
      gl.enableVertexAttribArray(0);
      gl.vertexAttribPointer(0, 3, gl.FLOAT, false, stride, 0);

      // Normal attribute
      gl.enableVertexAttribArray(1);
      gl.vertexAttribPointer(1, 3, gl.FLOAT, false, stride, 3 * 4);

      // TexCoord attribute
      gl.enableVertexAttribArray(2);
      gl.vertexAttribPointer(2, 2, gl.FLOAT, false, stride, 6 * 4);

      if (isSkinnedMesh) {
        // Joint indices attribute (uvec4 in shader, but stored as UBYTE4)
        gl.enableVertexAttribArray(3);
        gl.vertexAttribIPointer(3, 4, gl.UNSIGNED_BYTE, stride, 8 * 4);

        // Weights attribute (vec4)
        gl.enableVertexAttribArray(4);
        gl.vertexAttribPointer(4, 4, gl.FLOAT, false, stride, 8 * 4 + 4);
      } else {
        // Disable skinned attributes for static meshes
        gl.disableVertexAttribArray(3);
        gl.disableVertexAttribArray(4);
      }
    } else if (slot === 1) {
      // Instance buffer - contains mat4 per instance
      const mat4Stride = 16 * 4; // 16 floats per matrix

      // Matrix is passed as 4 vec4 attributes (locations 5, 6, 7, 8)
      // Each matrix is 16 floats (64 bytes), stored column-major
      for (let i = 0; i < 4; i++) {
        const attribLocation = 5 + i;
        gl.enableVertexAttribArray(attribLocation);
        gl.vertexAttribPointer(
          attribLocation,
          4,
          gl.FLOAT,
          false,
          mat4Stride,
          i * 16 // Offset for each column (4 floats * 4 bytes)
        );
        gl.vertexAttribDivisor(attribLocation, 1); // One per instance
      }
    }
  }

  function _webgl_apply_index_buffer(buffer_id) {
    const bufferObj = buffers.get(buffer_id);
    if (!bufferObj) {
      console.error("Invalid buffer ID for index buffer");
      return;
    }

    currentBindings.indexBuffer = bufferObj;
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, bufferObj.buffer);
  }

  function _webgl_apply_texture(slot, texture_view_id) {
    const textureObj = textures.get(texture_view_id);
    if (!textureObj) {
      console.error("Invalid texture view ID");
      return;
    }

    currentBindings.textures[slot] = textureObj;
    gl.activeTexture(gl.TEXTURE0 + slot);
    gl.bindTexture(gl.TEXTURE_2D, textureObj.texture);
  }

  function _webgl_apply_sampler(slot, sampler_id) {
    const samplerObj = samplers.get(sampler_id);
    if (!samplerObj) {
      console.error("Invalid sampler ID");
      return;
    }

    currentBindings.samplers[slot] = samplerObj;
    gl.bindSampler(slot, samplerObj.sampler);
  }

  function _webgl_apply_storage_buffer(slot, buffer_id) {
    const bufferObj = buffers.get(buffer_id);
    if (!bufferObj) {
      console.error("Invalid buffer ID for storage buffer");
      return;
    }

    currentBindings.storageBuffers[slot] = bufferObj;
  }

  function _webgl_apply_uniforms(slot, data_ptr, data_size) {
    if (!currentPipeline) {
      console.error("No pipeline bound for uniform application");
      return;
    }

    const location = currentPipeline.uniformArrayLocations[slot];
    if (location === undefined || location === null) {
      return;
    }

    // Read the data as a Float32Array since we're dealing with vec4 arrays
    const floatCount = data_size / 4; // 4 bytes per float
    const data = new Float32Array(wasmMemory.memory.buffer, data_ptr, floatCount);

    // Calculate how many vec4s we have
    const vec4Count = floatCount / 4;

    // Set the uniform array
    gl.uniform4fv(location, data);
  }

  function _webgl_draw(base_element, num_elements, num_instances) {
    if (!currentPipeline) {
      console.error("No pipeline bound for draw call");
      return;
    }

    if (num_instances > 1) {
      // Instanced drawing
      if (currentBindings.indexBuffer) {
        gl.drawElementsInstanced(
          currentPipeline.primitiveType,
          num_elements,
          currentPipeline.indexType,
          base_element * 4,
          num_instances
        );
      } else {
        gl.drawArraysInstanced(
          currentPipeline.primitiveType,
          base_element,
          num_elements,
          num_instances
        );
      }
    } else {
      // Regular drawing
      if (currentBindings.indexBuffer) {
        gl.drawElements(
          currentPipeline.primitiveType,
          num_elements,
          currentPipeline.indexType,
          base_element * 4
        );
      } else {
        gl.drawArrays(currentPipeline.primitiveType, base_element, num_elements);
      }
    }
  }

  function _webgl_commit() {}

  return {
    gl,
    _webgl_make_buffer,
    _webgl_destroy_buffer,
    _webgl_make_image,
    _webgl_make_texture_view,
    _webgl_destroy_image,
    _webgl_make_shader,
    _webgl_destroy_shader,
    _webgl_make_pipeline,
    _webgl_destroy_pipeline,
    _webgl_make_sampler,
    _webgl_begin_pass,
    _webgl_end_pass,
    _webgl_apply_pipeline,
    _webgl_apply_vertex_buffer,
    _webgl_apply_index_buffer,
    _webgl_apply_texture,
    _webgl_apply_sampler,
    _webgl_apply_storage_buffer,
    _webgl_apply_uniforms,
    _webgl_draw,
    _webgl_commit,
  };
}
