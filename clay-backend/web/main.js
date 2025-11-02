import {
  rectangleVertexShader,
  rectangleFragmentShader,
} from "./rectangle.glsl.js";
import { borderVertexShader, borderFragmentShader } from "./border.glsl.js";
import { imageVertexShader, imageFragmentShader } from "./image.glsl.js";
import { WasmMemoryInterface } from "./wasm.js";

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

const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });
const memInterface = new WasmMemoryInterface(memory);

// Image cache for texture loading
const imageCache = {}; // { url: { img: Image, texture: WebGLTexture, loaded: bool } }

const importObject = {
  env: {
    memory: memory,
    _os_log: (ptr, len) => {
      const message = memInterface.loadString(ptr, len);
      console.log("[WASM]:", message);
    },
    _os_canvas_width: () => {
      // Return CSS width (logical pixels), not physical pixels
      return canvas.clientWidth;
    },
    _os_canvas_height: () => {
      // Return CSS height (logical pixels), not physical pixels
      return canvas.clientHeight;
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
  renderer.rectangle.uniforms.rect = gl.getUniformLocation(renderer.rectangle.program, "u_rect");
  renderer.rectangle.uniforms.color = gl.getUniformLocation(renderer.rectangle.program, "u_color");
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

  const rectA_position = gl.getAttribLocation(renderer.rectangle.program, "a_position");
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
  renderer.border.uniforms.resolution = gl.getUniformLocation(renderer.border.program, "u_resolution");
  renderer.border.uniforms.rect = gl.getUniformLocation(renderer.border.program, "u_rect");
  renderer.border.uniforms.color = gl.getUniformLocation(renderer.border.program, "u_color");
  renderer.border.uniforms.cornerRadius = gl.getUniformLocation(renderer.border.program, "u_cornerRadius");
  renderer.border.uniforms.borderWidth = gl.getUniformLocation(renderer.border.program, "u_borderWidth");

  // Create border quad VAO and VBO (same geometry as rectangle)
  renderer.border.vao = gl.createVertexArray();
  gl.bindVertexArray(renderer.border.vao);

  renderer.border.vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, renderer.border.vbo);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const borderA_position = gl.getAttribLocation(renderer.border.program, "a_position");
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
  renderer.image.uniforms.rect = gl.getUniformLocation(renderer.image.program, "u_rect");
  renderer.image.uniforms.texture = gl.getUniformLocation(renderer.image.program, "u_texture");
  renderer.image.uniforms.tint = gl.getUniformLocation(renderer.image.program, "u_tint");
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

  const imageA_position = gl.getAttribLocation(renderer.image.program, "a_position");
  gl.enableVertexAttribArray(imageA_position);
  gl.vertexAttribPointer(imageA_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  console.log("WebGL2 initialized successfully (rectangles + borders + images)");
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
  wasmRuntime.exports.entrypoint(wasmRuntime.heapBase);

  // Start the render loop
  requestAnimationFrame(renderLoop);
}

loadWasm();
