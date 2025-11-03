import {
  rectangleVertexShader,
  rectangleFragmentShader,
} from "./rectangle.glsl.js";
import { borderVertexShader, borderFragmentShader } from "./border.glsl.js";
import { imageVertexShader, imageFragmentShader } from "./image.glsl.js";
import { textVertexShader, textFragmentShader } from "./text.glsl.js";
import { WasmMemoryInterface } from "./wasm.js";

export function createFileSystemFunctions(wasmMemory) {
  let nextFileReadId = 1;
  const pendingFileReads = new Map();
  const completedFileReads = new Map();

  const FileReadOpState = {
    NONE: 0,
    IN_PROGRESS: 1,
    COMPLETED: 2,
    ERROR: 3,
  };

  const FileReadResultCode = {
    SUCCESS: 0,
    FAIL: 1,
  };

  let nextWebPLoadId = 1;
  const pendingWebPLoads = new Map();
  const completedWebPLoads = new Map();

  function toAbsolutePath(path) {
    return path.startsWith("/") ? path : "/" + path;
  }

  function _os_start_read_file(filenamePtr, filenameLen) {
    const fileName = wasmMemory.loadString(filenamePtr, filenameLen);

    const id = nextFileReadId;
    nextFileReadId++;

    const op = { fileName, id };
    const result = {
      id,
      fileName,
      code: FileReadResultCode.FAIL,
      data: null,
    };

    pendingFileReads.set(op.id, op);

    fetch(toAbsolutePath(fileName))
      .then((response) => {
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const contentType = response.headers.get("content-type") || "";
        if (contentType.includes("text/html") && !fileName.endsWith(".html")) {
          throw new Error(`File not found: ${fileName} (got HTML fallback)`);
        }

        return response.arrayBuffer();
      })
      .then((data) => {
        result.data = new Uint8Array(data);
        result.code = FileReadResultCode.SUCCESS;
        completedFileReads.set(op.id, result);
        pendingFileReads.delete(op.id);
      })
      .catch((error) => {
        console.error(`Error reading file ${fileName}: ${error.message}`);
        result.code = FileReadResultCode.FAIL;
        completedFileReads.set(op.id, result);
        pendingFileReads.delete(op.id);
      });

    return id;
  }

  function _os_check_read_file(opId) {
    if (pendingFileReads.has(opId)) {
      return FileReadOpState.IN_PROGRESS;
    }

    if (completedFileReads.has(opId)) {
      const result = completedFileReads.get(opId);
      if (result?.code === FileReadResultCode.FAIL) {
        return FileReadOpState.ERROR;
      }
      return FileReadOpState.COMPLETED;
    }

    return FileReadOpState.NONE;
  }

  function _os_get_file_size(opId) {
    const state = _os_check_read_file(opId);
    if (state !== FileReadOpState.COMPLETED) {
      return -1;
    }
    const result = completedFileReads.get(opId);
    if (!result?.data) {
      return -1;
    }

    return result.data.byteLength;
  }

  function _os_get_file_data(opId, bufferPtr, bufferLen) {
    const state = _os_check_read_file(opId);
    if (state !== FileReadOpState.COMPLETED) {
      return -1;
    }

    const result = completedFileReads.get(opId);
    if (!result?.data) {
      return -1;
    }

    if (result.data.length > bufferLen) {
      return -1;
    }

    new Uint8Array(wasmMemory.memory.buffer, bufferPtr).set(result.data);

    completedFileReads.delete(opId);

    return 0;
  }

  function _os_start_webp_texture_load(
    filePathPtr,
    filePathLen,
    handleIdx,
    handleGen,
  ) {
    const filePath = wasmMemory.loadString(filePathPtr, filePathLen);
    const handle = { idx: handleIdx, gen: handleGen };
    const loadId = nextWebPLoadId++;

    console.log(
      `Starting WebP texture load: ${filePath}, handle:`,
      handle,
      "loadId:",
      loadId,
    );

    pendingWebPLoads.set(loadId, { filePath, handle });

    loadWebPTexture(filePath, handle, loadId);

    return loadId;
  }

  function _os_check_webp_texture_load(loadId) {
    if (pendingWebPLoads.has(loadId)) {
      return FileReadOpState.IN_PROGRESS;
    }

    if (completedWebPLoads.has(loadId)) {
      const result = completedWebPLoads.get(loadId);
      return result?.success
        ? FileReadOpState.COMPLETED
        : FileReadOpState.ERROR;
    }

    return FileReadOpState.NONE;
  }

  return {
    _os_start_read_file,
    _os_check_read_file,
    _os_get_file_size,
    _os_get_file_data,
    _os_start_webp_texture_load,
    _os_check_webp_texture_load,
  };
}

const canvas = document.getElementById("canvas");
const gl = canvas.getContext("webgl2", {
  antialias: true,
  premultipliedAlpha: false,
});

// Resize canvas to match display size with proper DPI
function resizeCanvasToDisplaySize() {
  const dpr = window.devicePixelRatio || 1;
  const displayWidth = canvas.clientWidth;
  const displayHeight = canvas.clientHeight;

  const needResize =
    canvas.width !== displayWidth * dpr ||
    canvas.height !== displayHeight * dpr;

  if (needResize) {
    canvas.width = displayWidth * dpr;
    canvas.height = displayHeight * dpr;

    // Update viewport
    gl.viewport(0, 0, canvas.width, canvas.height);

    console.log(
      `Canvas resized: ${displayWidth}x${displayHeight} CSS, ${canvas.width}x${canvas.height} pixels (DPR: ${dpr})`,
    );
  }

  return needResize;
}

// Initial resize
resizeCanvasToDisplaySize();

// Handle window resize
window.addEventListener("resize", resizeCanvasToDisplaySize);

const memory = new WebAssembly.Memory({ initial: 16384, maximum: 16384 });
const memInterface = new WasmMemoryInterface(memory);

// Image cache for texture loading
const imageCache = {}; // { url: { img: Image, texture: WebGLTexture, loaded: bool } }

const importObject = {
  env: {
    memory: memory,
    ...createFileSystemFunctions(memInterface),
    _os_cos: (x) => {
      return Math.cos(x);
    },
    _os_acos: (x) => {
      return Math.acos(x);
    },
    _os_pow: (x, y) => {
      return Math.pow(x, y);
    },
    _os_roundf: (x) => {
      return Math.round(x);
    },
    _os_log: (ptr, len) => {
      const message = memInterface.loadString(ptr, len);
      console.log(message);
    },
    _os_canvas_width: () => {
      // Return CSS width (logical pixels), not physical pixels
      return canvas.clientWidth;
    },
    _os_canvas_height: () => {
      // Return CSS height (logical pixels), not physical pixels
      return canvas.clientHeight;
    },
    _os_get_dpr: () => {
      return window.devicePixelRatio || 1.0;
    },
    _renderer_clear: (r, g, b, a) => {
      gl.clearColor(r, g, b, a);
      gl.clear(gl.COLOR_BUFFER_BIT);
    },
    _renderer_draw_rect: (
      x,
      y,
      width,
      height,
      r,
      g,
      b,
      a,
      cornerTL,
      cornerTR,
      cornerBL,
      cornerBR,
    ) => {
      const dpr = window.devicePixelRatio || 1;

      gl.useProgram(renderer.rectangle.program);
      gl.uniform2f(
        renderer.rectangle.uniforms.resolution,
        canvas.width,
        canvas.height,
      );
      gl.uniform4f(
        renderer.rectangle.uniforms.rect,
        x * dpr,
        y * dpr,
        width * dpr,
        height * dpr,
      );
      gl.uniform4f(
        renderer.rectangle.uniforms.color,
        r / 255.0,
        g / 255.0,
        b / 255.0,
        a / 255.0,
      );
      gl.uniform4f(
        renderer.rectangle.uniforms.cornerRadius,
        cornerTL * dpr,
        cornerTR * dpr,
        cornerBL * dpr,
        cornerBR * dpr,
      );
      gl.bindVertexArray(renderer.rectangle.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
    _renderer_draw_border: (
      x,
      y,
      width,
      height,
      r,
      g,
      b,
      a,
      cornerTL,
      cornerTR,
      cornerBL,
      cornerBR,
      borderL,
      borderR,
      borderT,
      borderB,
    ) => {
      const dpr = window.devicePixelRatio || 1;

      gl.useProgram(renderer.border.program);
      gl.uniform2f(
        renderer.border.uniforms.resolution,
        canvas.width,
        canvas.height,
      );
      gl.uniform4f(
        renderer.border.uniforms.rect,
        x * dpr,
        y * dpr,
        width * dpr,
        height * dpr,
      );
      gl.uniform4f(
        renderer.border.uniforms.color,
        r / 255.0,
        g / 255.0,
        b / 255.0,
        a / 255.0,
      );
      gl.uniform4f(
        renderer.border.uniforms.cornerRadius,
        cornerTL * dpr,
        cornerTR * dpr,
        cornerBL * dpr,
        cornerBR * dpr,
      );
      gl.uniform4f(
        renderer.border.uniforms.borderWidth,
        borderL * dpr,
        borderR * dpr,
        borderT * dpr,
        borderB * dpr,
      );
      gl.bindVertexArray(renderer.border.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
    _renderer_scissor_start: (x, y, width, height) => {
      const dpr = window.devicePixelRatio || 1;

      // WebGL scissor uses bottom-left origin, but Clay uses top-left
      // Need to flip Y coordinate
      const scissorX = x * dpr;
      const scissorY = canvas.height - (y + height) * dpr;
      const scissorW = width * dpr;
      const scissorH = height * dpr;

      gl.enable(gl.SCISSOR_TEST);
      gl.scissor(scissorX, scissorY, scissorW, scissorH);
    },
    _renderer_scissor_end: () => {
      gl.disable(gl.SCISSOR_TEST);
    },
    _renderer_draw_image: (
      x,
      y,
      width,
      height,
      imageDataPtr,
      r,
      g,
      b,
      a,
      cornerTL,
      cornerTR,
      cornerBL,
      cornerBR,
    ) => {
      // 1. Read null-terminated C string from WASM memory
      const url = memInterface.loadCstringDirect(imageDataPtr);

      if (!url) {
        console.warn("Image URL is null");
        return;
      }

      // 2. Load image if not cached
      if (!imageCache[url]) {
        imageCache[url] = {
          img: new Image(),
          texture: null,
          loaded: false,
        };

        imageCache[url].img.onload = () => {
          // Create WebGL texture
          const texture = gl.createTexture();
          gl.bindTexture(gl.TEXTURE_2D, texture);

          // Upload image data
          gl.texImage2D(
            gl.TEXTURE_2D,
            0,
            gl.RGBA,
            gl.RGBA,
            gl.UNSIGNED_BYTE,
            imageCache[url].img,
          );

          // Texture parameters (no mipmaps for UI images)
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

          imageCache[url].texture = texture;
          imageCache[url].loaded = true;
        };

        imageCache[url].img.onerror = () => {
          console.error(`Failed to load image: ${url}`);
        };

        // Enable CORS for cross-origin images (required for WebGL textures)
        imageCache[url].img.crossOrigin = "anonymous";
        imageCache[url].img.src = url;
        return; // Skip this frame
      }

      // 3. Skip if not loaded yet
      if (!imageCache[url].loaded) return;

      // 4. Render textured quad
      const dpr = window.devicePixelRatio || 1;

      // Default tint to white if backgroundColor is (0,0,0,0)
      let tintR = r,
        tintG = g,
        tintB = b,
        tintA = a;
      if (r === 0 && g === 0 && b === 0 && a === 0) {
        tintR = tintG = tintB = tintA = 255;
      }

      gl.useProgram(renderer.image.program);
      gl.uniform2f(
        renderer.image.uniforms.resolution,
        canvas.width,
        canvas.height,
      );
      gl.uniform4f(
        renderer.image.uniforms.rect,
        x * dpr,
        y * dpr,
        width * dpr,
        height * dpr,
      );
      gl.uniform4f(
        renderer.image.uniforms.tint,
        tintR / 255.0,
        tintG / 255.0,
        tintB / 255.0,
        tintA / 255.0,
      );
      gl.uniform4f(
        renderer.image.uniforms.cornerRadius,
        cornerTL * dpr,
        cornerTR * dpr,
        cornerBL * dpr,
        cornerBR * dpr,
      );

      // Bind texture to unit 0
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, imageCache[url].texture);
      gl.uniform1i(renderer.image.uniforms.texture, 0);

      gl.bindVertexArray(renderer.image.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
    _renderer_draw_glyph: (x, y, width, height, bitmapPtr, r, g, b, a) => {
      // Ensure integers (defensive)
      const w = Math.floor(width);
      const h = Math.floor(height);

      if (w === 0 || h === 0) return;

      // Copy bitmap from WASM memory (single-channel grayscale)
      const bitmap = memInterface.loadU8Array(bitmapPtr, bitmapPtr + w * h);

      // Create temporary texture
      const texture = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, texture);

      // Set pixel store to 1-byte alignment (default is 4)
      // This is critical for odd-width textures
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

      // Upload as single-channel RED texture (GL_ALPHA deprecated in WebGL2)
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.R8, // Internal format: single-channel 8-bit
        w,
        h,
        0,
        gl.RED, // Format: red channel
        gl.UNSIGNED_BYTE,
        bitmap,
      );

      // Linear filtering for smooth text
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

      // Use text shader
      gl.useProgram(renderer.text.program);

      // Set uniforms
      // Note: x, y are already in physical pixels from C side
      // width, height are the actual bitmap dimensions (already physical pixels)
      gl.uniform2f(
        renderer.text.uniforms.resolution,
        canvas.width,
        canvas.height,
      );
      gl.uniform4f(
        renderer.text.uniforms.rect,
        x,  // Already physical pixels
        y,  // Already physical pixels
        w,  // Bitmap width (physical pixels)
        h,  // Bitmap height (physical pixels)
      );
      gl.uniform4f(renderer.text.uniforms.color, r, g, b, a);

      // Bind texture
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.uniform1i(renderer.text.uniforms.texture, 0);

      // Draw quad
      gl.bindVertexArray(renderer.text.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);

      // Cleanup (no caching yet)
      gl.deleteTexture(texture);
    },
  },
};

// WebGL2 Renderer state
const renderer = {
  rectangle: {
    program: null,
    vao: null,
    vbo: null,
    uniforms: {
      resolution: null,
      rect: null,
      color: null,
      cornerRadius: null,
    },
  },
  border: {
    program: null,
    vao: null,
    vbo: null,
    uniforms: {
      resolution: null,
      rect: null,
      color: null,
      cornerRadius: null,
      borderWidth: null,
    },
  },
  image: {
    program: null,
    vao: null,
    vbo: null,
    uniforms: {
      resolution: null,
      rect: null,
      texture: null,
      tint: null,
      cornerRadius: null,
    },
  },
  text: {
    program: null,
    vao: null,
    vbo: null,
    uniforms: {
      resolution: null,
      rect: null,
      texture: null,
      color: null,
    },
  },
};

// WASM runtime state
const wasmRuntime = {
  exports: null,
  heapBase: 0,
  previousFrameTime: 0,
};

// Initialize WebGL2 for rendering
function initWebGL2() {
  // Set viewport
  gl.viewport(0, 0, canvas.width, canvas.height);

  // Set clear color (black background)
  gl.clearColor(0.0, 0.0, 0.0, 1.0);

  // Enable blending for alpha transparency
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  // Compile rectangle shaders
  const rectVertShader = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(rectVertShader, rectangleVertexShader);
  gl.compileShader(rectVertShader);

  if (!gl.getShaderParameter(rectVertShader, gl.COMPILE_STATUS)) {
    console.error(
      "Rectangle vertex shader compile error:",
      gl.getShaderInfoLog(rectVertShader),
    );
    return;
  }

  const rectFragShader = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(rectFragShader, rectangleFragmentShader);
  gl.compileShader(rectFragShader);

  if (!gl.getShaderParameter(rectFragShader, gl.COMPILE_STATUS)) {
    console.error(
      "Rectangle fragment shader compile error:",
      gl.getShaderInfoLog(rectFragShader),
    );
    return;
  }

  // Link rectangle program
  renderer.rectangle.program = gl.createProgram();
  gl.attachShader(renderer.rectangle.program, rectVertShader);
  gl.attachShader(renderer.rectangle.program, rectFragShader);
  gl.linkProgram(renderer.rectangle.program);

  if (!gl.getProgramParameter(renderer.rectangle.program, gl.LINK_STATUS)) {
    console.error(
      "Rectangle program link error:",
      gl.getProgramInfoLog(renderer.rectangle.program),
    );
    return;
  }

  // Get rectangle uniform locations
  renderer.rectangle.uniforms.resolution = gl.getUniformLocation(
    renderer.rectangle.program,
    "u_resolution",
  );
  renderer.rectangle.uniforms.rect = gl.getUniformLocation(
    renderer.rectangle.program,
    "u_rect",
  );
  renderer.rectangle.uniforms.color = gl.getUniformLocation(
    renderer.rectangle.program,
    "u_color",
  );
  renderer.rectangle.uniforms.cornerRadius = gl.getUniformLocation(
    renderer.rectangle.program,
    "u_cornerRadius",
  );

  // Create rectangle quad geometry (normalized 0-1)
  const quadVertices = new Float32Array([
    0,
    0, // bottom-left
    1,
    0, // bottom-right
    0,
    1, // top-left
    1,
    0, // bottom-right
    1,
    1, // top-right
    0,
    1, // top-left
  ]);

  // Create rectangle VAO and VBO
  renderer.rectangle.vao = gl.createVertexArray();
  gl.bindVertexArray(renderer.rectangle.vao);

  renderer.rectangle.vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, renderer.rectangle.vbo);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const rectA_position = gl.getAttribLocation(
    renderer.rectangle.program,
    "a_position",
  );
  gl.enableVertexAttribArray(rectA_position);
  gl.vertexAttribPointer(rectA_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  // ===== Border Shader Program =====

  // Compile border shaders
  const borderVertShader = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(borderVertShader, borderVertexShader);
  gl.compileShader(borderVertShader);

  if (!gl.getShaderParameter(borderVertShader, gl.COMPILE_STATUS)) {
    console.error(
      "Border vertex shader compile error:",
      gl.getShaderInfoLog(borderVertShader),
    );
    return;
  }

  const borderFragShader = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(borderFragShader, borderFragmentShader);
  gl.compileShader(borderFragShader);

  if (!gl.getShaderParameter(borderFragShader, gl.COMPILE_STATUS)) {
    console.error(
      "Border fragment shader compile error:",
      gl.getShaderInfoLog(borderFragShader),
    );
    return;
  }

  // Link border program
  renderer.border.program = gl.createProgram();
  gl.attachShader(renderer.border.program, borderVertShader);
  gl.attachShader(renderer.border.program, borderFragShader);
  gl.linkProgram(renderer.border.program);

  if (!gl.getProgramParameter(renderer.border.program, gl.LINK_STATUS)) {
    console.error(
      "Border program link error:",
      gl.getProgramInfoLog(renderer.border.program),
    );
    return;
  }

  // Get border uniform locations
  renderer.border.uniforms.resolution = gl.getUniformLocation(
    renderer.border.program,
    "u_resolution",
  );
  renderer.border.uniforms.rect = gl.getUniformLocation(
    renderer.border.program,
    "u_rect",
  );
  renderer.border.uniforms.color = gl.getUniformLocation(
    renderer.border.program,
    "u_color",
  );
  renderer.border.uniforms.cornerRadius = gl.getUniformLocation(
    renderer.border.program,
    "u_cornerRadius",
  );
  renderer.border.uniforms.borderWidth = gl.getUniformLocation(
    renderer.border.program,
    "u_borderWidth",
  );

  // Create border quad VAO and VBO (same geometry as rectangle)
  renderer.border.vao = gl.createVertexArray();
  gl.bindVertexArray(renderer.border.vao);

  renderer.border.vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, renderer.border.vbo);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const borderA_position = gl.getAttribLocation(
    renderer.border.program,
    "a_position",
  );
  gl.enableVertexAttribArray(borderA_position);
  gl.vertexAttribPointer(borderA_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  // ===== Image Shader Program =====

  // Compile image shaders
  const imageVertShader = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(imageVertShader, imageVertexShader);
  gl.compileShader(imageVertShader);

  if (!gl.getShaderParameter(imageVertShader, gl.COMPILE_STATUS)) {
    console.error(
      "Image vertex shader compile error:",
      gl.getShaderInfoLog(imageVertShader),
    );
    return;
  }

  const imageFragShader = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(imageFragShader, imageFragmentShader);
  gl.compileShader(imageFragShader);

  if (!gl.getShaderParameter(imageFragShader, gl.COMPILE_STATUS)) {
    console.error(
      "Image fragment shader compile error:",
      gl.getShaderInfoLog(imageFragShader),
    );
    return;
  }

  // Link image program
  renderer.image.program = gl.createProgram();
  gl.attachShader(renderer.image.program, imageVertShader);
  gl.attachShader(renderer.image.program, imageFragShader);
  gl.linkProgram(renderer.image.program);

  if (!gl.getProgramParameter(renderer.image.program, gl.LINK_STATUS)) {
    console.error(
      "Image program link error:",
      gl.getProgramInfoLog(renderer.image.program),
    );
    return;
  }

  // Get image uniform locations
  renderer.image.uniforms.resolution = gl.getUniformLocation(
    renderer.image.program,
    "u_resolution",
  );
  renderer.image.uniforms.rect = gl.getUniformLocation(
    renderer.image.program,
    "u_rect",
  );
  renderer.image.uniforms.texture = gl.getUniformLocation(
    renderer.image.program,
    "u_texture",
  );
  renderer.image.uniforms.tint = gl.getUniformLocation(
    renderer.image.program,
    "u_tint",
  );
  renderer.image.uniforms.cornerRadius = gl.getUniformLocation(
    renderer.image.program,
    "u_cornerRadius",
  );

  // Create image quad VAO and VBO (same geometry as rectangle)
  renderer.image.vao = gl.createVertexArray();
  gl.bindVertexArray(renderer.image.vao);

  renderer.image.vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, renderer.image.vbo);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const imageA_position = gl.getAttribLocation(
    renderer.image.program,
    "a_position",
  );
  gl.enableVertexAttribArray(imageA_position);
  gl.vertexAttribPointer(imageA_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  // ===== Text Shader Program =====

  // Compile text shaders
  const textVertShader = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(textVertShader, textVertexShader);
  gl.compileShader(textVertShader);

  if (!gl.getShaderParameter(textVertShader, gl.COMPILE_STATUS)) {
    console.error(
      "Text vertex shader compile error:",
      gl.getShaderInfoLog(textVertShader),
    );
    return;
  }

  const textFragShader = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(textFragShader, textFragmentShader);
  gl.compileShader(textFragShader);

  if (!gl.getShaderParameter(textFragShader, gl.COMPILE_STATUS)) {
    console.error(
      "Text fragment shader compile error:",
      gl.getShaderInfoLog(textFragShader),
    );
    return;
  }

  // Link text program
  renderer.text.program = gl.createProgram();
  gl.attachShader(renderer.text.program, textVertShader);
  gl.attachShader(renderer.text.program, textFragShader);
  gl.linkProgram(renderer.text.program);

  if (!gl.getProgramParameter(renderer.text.program, gl.LINK_STATUS)) {
    console.error(
      "Text program link error:",
      gl.getProgramInfoLog(renderer.text.program),
    );
    return;
  }

  // Get text uniform locations
  renderer.text.uniforms.resolution = gl.getUniformLocation(
    renderer.text.program,
    "u_resolution",
  );
  renderer.text.uniforms.rect = gl.getUniformLocation(
    renderer.text.program,
    "u_rect",
  );
  renderer.text.uniforms.texture = gl.getUniformLocation(
    renderer.text.program,
    "u_texture",
  );
  renderer.text.uniforms.color = gl.getUniformLocation(
    renderer.text.program,
    "u_color",
  );

  // Create text quad VAO and VBO (same geometry as others)
  renderer.text.vao = gl.createVertexArray();
  gl.bindVertexArray(renderer.text.vao);

  renderer.text.vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, renderer.text.vbo);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const textA_position = gl.getAttribLocation(
    renderer.text.program,
    "a_position",
  );
  gl.enableVertexAttribArray(textA_position);
  gl.vertexAttribPointer(textA_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  console.log(
    "WebGL2 initialized successfully (rectangles + borders + images + text)",
  );
}

function renderLoop(currentTime) {
  // Calculate delta time in seconds
  const deltaTime = wasmRuntime.previousFrameTime
    ? (currentTime - wasmRuntime.previousFrameTime) / 1000
    : 0;
  wasmRuntime.previousFrameTime = currentTime;

  // Ensure canvas is at proper resolution
  resizeCanvasToDisplaySize();

  // Call WASM update_and_render
  wasmRuntime.exports.update_and_render(wasmRuntime.heapBase);

  // Request next frame
  requestAnimationFrame(renderLoop);
}

// Load a file from URL and write it to WASM memory
async function loadFile(url, bufferPtr, bufferSize) {
  try {
    console.log(`Loading file: ${url}`);
    const response = await fetch(url);

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    const arrayBuffer = await response.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);

    console.log(`File loaded: ${uint8Array.length} bytes`);

    // Check if buffer is large enough
    if (uint8Array.length > bufferSize) {
      console.error(`Buffer too small: ${bufferSize} < ${uint8Array.length}`);
      // Call error callback
      wasmRuntime.exports.on_file_load_error(bufferPtr, 1); // 1 = BUFFER_TOO_SMALL
      return;
    }

    // Write data to WASM memory
    const wasmMemory = new Uint8Array(memory.buffer);
    wasmMemory.set(uint8Array, bufferPtr);

    // Call success callback with actual size
    wasmRuntime.exports.on_file_load_success(bufferPtr, uint8Array.length);
  } catch (error) {
    console.error(`Failed to load file ${url}:`, error);
    // Call error callback
    wasmRuntime.exports.on_file_load_error(bufferPtr, 2); // 2 = FETCH_FAILED
  }
}

async function loadWasm() {
  const response = await fetch("app.wasm");
  const wasmBytes = await response.arrayBuffer();
  const wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);

  wasmRuntime.exports = wasmModule.instance.exports;
  window.wasmExports = wasmRuntime.exports;

  // Get heap base pointer
  wasmRuntime.heapBase = wasmRuntime.exports.os_get_heap_base();

  // Initialize WebGL2
  initWebGL2();

  // Call entrypoint with memory pointer
  wasmRuntime.exports.entrypoint(
    wasmRuntime.heapBase,
    BigInt(memory.buffer.byteLength - wasmRuntime.heapBase),
  );

  // Start the render loop
  requestAnimationFrame(renderLoop);
}

loadWasm();
