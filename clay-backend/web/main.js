class WasmMemoryInterface {
  constructor(memory) {
    this.memory = memory;
    // Size (in bytes) of the integer type, should be 4 on `js_wasm32` and 8 on `js_wasm64p32`
    this.intSize = 4;
  }

  get mem() {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new DataView(this.memory.buffer);
  }

  loadF32Array(addr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Float32Array(this.memory.buffer, addr, len);
  }

  loadF64Array(addr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Float64Array(this.memory.buffer, addr, len);
  }

  loadU32Array(addr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Uint32Array(this.memory.buffer, addr, len);
  }

  loadI32Array(addr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Int32Array(this.memory.buffer, addr, len);
  }

  loadI16Array(addr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Int16Array(this.memory.buffer, addr, len);
  }

  loadU8(addr) {
    return this.mem.getUint8(addr);
  }
  loadI8(addr) {
    return this.mem.getInt8(addr);
  }
  loadU16(addr) {
    return this.mem.getUint16(addr, true);
  }
  loadI16(addr) {
    return this.mem.getInt16(addr, true);
  }
  loadU32(addr) {
    return this.mem.getUint32(addr, true);
  }
  loadI32(addr) {
    return this.mem.getInt32(addr, true);
  }
  loadU64(addr) {
    const lo = this.mem.getUint32(addr + 0, true);
    const hi = this.mem.getUint32(addr + 4, true);
    return lo + hi * 4294967296;
  }
  loadI64(addr) {
    const lo = this.mem.getUint32(addr + 0, true);
    const hi = this.mem.getInt32(addr + 4, true);
    return lo + hi * 4294967296;
  }
  loadF32(addr) {
    return this.mem.getFloat32(addr, true);
  }
  loadF64(addr) {
    return this.mem.getFloat64(addr, true);
  }
  loadInt(addr) {
    if (this.intSize === 8) {
      return this.loadI64(addr);
    } else if (this.intSize === 4) {
      return this.loadI32(addr);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  loadUint(addr) {
    if (this.intSize === 8) {
      return this.loadU64(addr);
    } else if (this.intSize === 4) {
      return this.loadU32(addr);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  loadPtr(addr) {
    return this.loadU32(addr);
  }

  loadB32(addr) {
    return this.loadU32(addr) !== 0;
  }

  loadBytes(ptr, len) {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Uint8Array(this.memory.buffer, ptr, Number(len));
  }

  loadString(ptr, len) {
    const bytes = this.loadBytes(ptr, Number(len));
    return new TextDecoder().decode(bytes);
  }

  loadCstring(ptr) {
    return this.loadCstringDirect(this.loadPtr(ptr));
  }

  loadCstringDirect(start) {
    if (start === 0) {
      return null;
    }
    let len = 0;
    for (; this.mem.getUint8(start + len) !== 0; len += 1) {}
    return this.loadString(start, len);
  }

  storeU8(addr, value) {
    this.mem.setUint8(addr, value);
  }
  storeI8(addr, value) {
    this.mem.setInt8(addr, value);
  }
  storeU16(addr, value) {
    this.mem.setUint16(addr, value, true);
  }
  storeI16(addr, value) {
    this.mem.setInt16(addr, value, true);
  }
  storeU32(addr, value) {
    this.mem.setUint32(addr, value, true);
  }
  storeI32(addr, value) {
    this.mem.setInt32(addr, value, true);
  }
  storeU64(addr, value) {
    this.mem.setUint32(addr + 0, Number(value), true);

    let div = 4294967296;
    if (typeof value === "bigint") {
      div = BigInt(div);
    }

    this.mem.setUint32(addr + 4, Math.floor(Number(value / div)), true);
  }
  storeI64(addr, value) {
    this.mem.setUint32(addr + 0, Number(value), true);

    let div = 4294967296;
    if (typeof value === "bigint") {
      div = BigInt(div);
    }

    this.mem.setInt32(addr + 4, Math.floor(Number(value / div)), true);
  }
  storeF32(addr, value) {
    this.mem.setFloat32(addr, value, true);
  }
  storeF64(addr, value) {
    this.mem.setFloat64(addr, value, true);
  }
  storeInt(addr, value) {
    if (this.intSize === 8) {
      this.storeI64(addr, value);
    } else if (this.intSize === 4) {
      this.storeI32(addr, value);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  storeUint(addr, value) {
    if (this.intSize === 8) {
      this.storeU64(addr, value);
    } else if (this.intSize === 4) {
      this.storeU32(addr, value);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }

  // Returned length might not be the same as `value.length` if non-ascii strings are given.
  storeString(addr, value) {
    const src = new TextEncoder().encode(value);
    const dst = new Uint8Array(this.memory.buffer, addr, src.length);
    dst.set(src);
    return src.length;
  }
}

const canvas = document.getElementById("canvas");
const gl = canvas.getContext("webgl2", {
  antialias: true,
  premultipliedAlpha: false,
});

const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });
const memInterface = new WasmMemoryInterface(memory);

const importObject = {
  env: {
    memory: memory,
    _os_log: (ptr, len) => {
      const message = memInterface.loadString(ptr, len);
      console.log("[WASM]:", message);
    },
    _os_canvas_width: () => {
      return canvas.width;
    },
    _os_canvas_height: () => {
      return canvas.height;
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
      gl.useProgram(rectangleProgram);
      gl.uniform2f(u_resolution, canvas.width, canvas.height);
      gl.uniform4f(u_rect, x, y, width, height);
      gl.uniform4f(u_color, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
      gl.uniform4f(u_cornerRadius, cornerTL, cornerTR, cornerBL, cornerBR);
      gl.bindVertexArray(quadVAO);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
  },
};

let wasmExports = null;
let heapBase = 0;
let previousFrameTime = 0;

// WebGL2 state
let rectangleProgram = null;
let quadVAO = null;
let quadVBO = null;
let u_resolution = null;
let u_rect = null;
let u_color = null;
let u_cornerRadius = null;

// Initialize WebGL2 for rendering
function initWebGL2() {
  // Set viewport
  gl.viewport(0, 0, canvas.width, canvas.height);

  // Set clear color (black background)
  gl.clearColor(0.0, 0.0, 0.0, 1.0);

  // Enable blending for alpha transparency
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  // Create shader program for rectangles
  const vertexShaderSource = `#version 300 es
    precision mediump float;

    in vec2 a_position;
    uniform vec2 u_resolution;
    uniform vec4 u_rect;

    out vec2 v_position;  // Position in rect local space

    void main() {
      vec2 position = a_position * u_rect.zw + u_rect.xy;
      v_position = a_position * u_rect.zw;  // Local position for fragment shader

      vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
      gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
    }
  `;

  const fragmentShaderSource = `#version 300 es
    precision mediump float;

    uniform mediump vec4 u_color;
    uniform mediump vec4 u_rect;         // x, y, width, height
    uniform mediump vec4 u_cornerRadius; // topLeft, topRight, bottomLeft, bottomRight

    in vec2 v_position;  // Pixel position in rect space (0-width, 0-height)

    out vec4 fragColor;

    // SDF for rounded rectangle
    float roundedBoxSDF(vec2 p, vec2 size, vec4 radius) {
      // Select radius based on quadrant
      float r = (p.x < 0.0) ?
                ((p.y < 0.0) ? radius.x : radius.z) :  // topLeft : bottomLeft
                ((p.y < 0.0) ? radius.y : radius.w);   // topRight : bottomRight

      vec2 q = abs(p) - size + r;
      return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
    }

    void main() {
      // Convert to centered coordinates
      vec2 halfSize = u_rect.zw * 0.5;
      vec2 center = v_position - halfSize;

      // Calculate SDF distance
      float dist = roundedBoxSDF(center, halfSize, u_cornerRadius);

      // High-quality antialiasing using screen-space derivatives
      // fwidth gives us the rate of change per pixel
      float edgeWidth = length(vec2(dFdx(dist), dFdy(dist)));

      // Clamp to avoid artifacts on very small shapes
      edgeWidth = max(edgeWidth, 0.5);

      // Smooth transition with high quality
      float alpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, dist);

      fragColor = vec4(u_color.rgb, u_color.a * alpha);
    }
  `;

  // Compile shaders
  const vertexShader = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(vertexShader, vertexShaderSource);
  gl.compileShader(vertexShader);

  if (!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
    console.error(
      "Vertex shader compile error:",
      gl.getShaderInfoLog(vertexShader),
    );
    return;
  }

  const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(fragmentShader, fragmentShaderSource);
  gl.compileShader(fragmentShader);

  if (!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS)) {
    console.error(
      "Fragment shader compile error:",
      gl.getShaderInfoLog(fragmentShader),
    );
    return;
  }

  // Link program
  rectangleProgram = gl.createProgram();
  gl.attachShader(rectangleProgram, vertexShader);
  gl.attachShader(rectangleProgram, fragmentShader);
  gl.linkProgram(rectangleProgram);

  if (!gl.getProgramParameter(rectangleProgram, gl.LINK_STATUS)) {
    console.error(
      "Program link error:",
      gl.getProgramInfoLog(rectangleProgram),
    );
    return;
  }

  // Get uniform locations
  u_resolution = gl.getUniformLocation(rectangleProgram, "u_resolution");
  u_rect = gl.getUniformLocation(rectangleProgram, "u_rect");
  u_color = gl.getUniformLocation(rectangleProgram, "u_color");
  u_cornerRadius = gl.getUniformLocation(rectangleProgram, "u_cornerRadius");

  // Create quad geometry (normalized 0-1)
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

  // Create VAO and VBO
  quadVAO = gl.createVertexArray();
  gl.bindVertexArray(quadVAO);

  quadVBO = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, quadVBO);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  const a_position = gl.getAttribLocation(rectangleProgram, "a_position");
  gl.enableVertexAttribArray(a_position);
  gl.vertexAttribPointer(a_position, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  console.log("WebGL2 initialized successfully");
}

function renderLoop(currentTime) {
  // Calculate delta time in seconds
  const deltaTime = previousFrameTime
    ? (currentTime - previousFrameTime) / 1000
    : 0;
  previousFrameTime = currentTime;

  // Call WASM update_and_render
  wasmExports.update_and_render(heapBase);

  // Request next frame
  requestAnimationFrame(renderLoop);
}

async function loadWasm() {
  const response = await fetch("app.wasm");
  const wasmBytes = await response.arrayBuffer();
  const wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);

  wasmExports = wasmModule.instance.exports;
  window.wasmExports = wasmExports;

  // Get heap base pointer
  heapBase = wasmExports.os_get_heap_base();

  // Initialize WebGL2
  initWebGL2();

  // Call entrypoint with memory pointer
  wasmExports.entrypoint(heapBase);

  // Start the render loop
  requestAnimationFrame(renderLoop);
}

loadWasm();
