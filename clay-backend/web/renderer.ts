import {
  rectangleVertexShader,
  rectangleFragmentShader,
} from "./rectangle.glsl.js";
import { borderVertexShader, borderFragmentShader } from "./border.glsl.js";
import { imageVertexShader, imageFragmentShader } from "./image.glsl.js";
import { textVertexShader, textFragmentShader } from "./text.glsl.js";
import type { WasmMemoryInterface } from "./wasm.ts";

interface ShaderUniforms {
  resolution: WebGLUniformLocation | null;
  rect: WebGLUniformLocation | null;
  color: WebGLUniformLocation | null;
  cornerRadius: WebGLUniformLocation | null;
}

interface BorderUniforms extends ShaderUniforms {
  borderWidth: WebGLUniformLocation | null;
}

interface ImageUniforms extends ShaderUniforms {
  texture: WebGLUniformLocation | null;
  tint: WebGLUniformLocation | null;
}

interface TextUniforms {
  resolution: WebGLUniformLocation | null;
  rect: WebGLUniformLocation | null;
  texture: WebGLUniformLocation | null;
  color: WebGLUniformLocation | null;
  distanceRange: WebGLUniformLocation | null;
  fontSize: WebGLUniformLocation | null;
  uvBounds: WebGLUniformLocation | null;
}

interface ShaderProgram {
  program: WebGLProgram | null;
  vao: WebGLVertexArrayObject | null;
  vbo: WebGLBuffer | null;
  uniforms: ShaderUniforms;
}

interface BorderProgram extends ShaderProgram {
  uniforms: BorderUniforms;
}

interface ImageProgram extends ShaderProgram {
  uniforms: ImageUniforms;
}

interface TextProgram {
  program: WebGLProgram | null;
  vao: WebGLVertexArrayObject | null;
  vbo: WebGLBuffer | null;
  atlasTexture: WebGLTexture | null;
  uniforms: TextUniforms;
}

interface Renderer {
  canvas: HTMLCanvasElement;
  gl: WebGL2RenderingContext;
  rectangle: ShaderProgram;
  border: BorderProgram;
  image: ImageProgram;
  text: TextProgram;
}

interface ImageCacheEntry {
  img: HTMLImageElement;
  texture: WebGLTexture | null;
  loaded: boolean;
}

interface ImageCache {
  [url: string]: ImageCacheEntry;
}

// WebGL2 Renderer state
const renderer: Renderer = {
  canvas: document.getElementById("canvas") as HTMLCanvasElement,
  gl: (document.getElementById("canvas") as HTMLCanvasElement).getContext(
    "webgl2",
    {
      antialias: true,
      premultipliedAlpha: false,
    },
  ) as WebGL2RenderingContext,
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
      color: null,
    },
  },
  text: {
    program: null,
    vao: null,
    vbo: null,
    atlasTexture: null,
    uniforms: {
      resolution: null,
      rect: null,
      texture: null,
      color: null,
      distanceRange: null,
      fontSize: null,
      uvBounds: null,
    },
  },
};

const imageCache: ImageCache = {};

// Initialize WebGL2 for rendering
export function initWebGL2(): void {
  const { gl, canvas } = renderer;
  // Set viewport
  gl.viewport(0, 0, canvas.width, canvas.height);

  // Set clear color (black background)
  gl.clearColor(0.0, 0.0, 0.0, 1.0);

  // Enable blending for alpha transparency
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  // Compile rectangle shaders
  const rectVertShader = gl.createShader(gl.VERTEX_SHADER);
  if (!rectVertShader) return;
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
  if (!rectFragShader) return;
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
  if (!renderer.rectangle.program) return;
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
  if (!borderVertShader) return;
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
  if (!borderFragShader) return;
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
  if (!renderer.border.program) return;
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
  if (!imageVertShader) return;
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
  if (!imageFragShader) return;
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
  if (!renderer.image.program) return;
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
  if (!textVertShader) return;
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
  if (!textFragShader) return;
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
  if (!renderer.text.program) return;
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
  renderer.text.uniforms.distanceRange = gl.getUniformLocation(
    renderer.text.program,
    "u_distanceRange",
  );
  renderer.text.uniforms.fontSize = gl.getUniformLocation(
    renderer.text.program,
    "u_fontSize",
  );
  renderer.text.uniforms.uvBounds = gl.getUniformLocation(
    renderer.text.program,
    "u_uvBounds",
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

// Resize canvas to match display size with proper DPI
export function resizeCanvasToDisplaySize(): boolean {
  const { gl, canvas } = renderer;
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

export function createRendererOSFunctions(memInterface: WasmMemoryInterface) {
  const { gl, canvas } = renderer;
  return {
    _renderer_clear: (r: number, g: number, b: number, a: number): void => {
      gl.clearColor(r, g, b, a);
      gl.clear(gl.COLOR_BUFFER_BIT);
    },
    _renderer_draw_rect: (
      x: number,
      y: number,
      width: number,
      height: number,
      r: number,
      g: number,
      b: number,
      a: number,
      cornerTL: number,
      cornerTR: number,
      cornerBL: number,
      cornerBR: number,
    ): void => {
      //todo: pass dpr?
      const dpr = window.devicePixelRatio || 1;

      gl.useProgram(renderer.rectangle.program);
      //todo: pack uniforms?
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
      x: number,
      y: number,
      width: number,
      height: number,
      r: number,
      g: number,
      b: number,
      a: number,
      cornerTL: number,
      cornerTR: number,
      cornerBL: number,
      cornerBR: number,
      borderL: number,
      borderR: number,
      borderT: number,
      borderB: number,
    ): void => {
      //todo: consolidate with rectangle rendering
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
    _renderer_scissor_start: (
      x: number,
      y: number,
      width: number,
      height: number,
    ): void => {
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
    _renderer_scissor_end: (): void => {
      gl.disable(gl.SCISSOR_TEST);
    },
    _renderer_draw_image: (
      x: number,
      y: number,
      width: number,
      height: number,
      imageDataPtr: number,
      r: number,
      g: number,
      b: number,
      a: number,
      cornerTL: number,
      cornerTR: number,
      cornerBL: number,
      cornerBR: number,
    ): void => {
      //todo: pass image bytes instead instead of url
      // 1. Read null-terminated C string from WASM memory
      const url = memInterface.loadCstringDirect(imageDataPtr);

      if (!url) {
        console.warn("Image URL is null");
        return;
      }

      //todo: image atlas on the C side
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

      //todo: consolidate
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

      //todo: same vao as rectangle no?
      gl.bindVertexArray(renderer.image.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
    _renderer_upload_msdf_atlas: (
      imageDataPtr: number,
      width: number,
      height: number,
      channels: number,
    ): void => {
      console.log(
        `Uploading MSDF atlas: ${width}x${height}, channels=${channels}`,
      );

      // Read image data from WASM memory
      const imageData = memInterface.loadU8Array(
        imageDataPtr,
        imageDataPtr + width * height * channels,
      );

      // Create and upload texture
      const texture = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, texture);

      // Upload RGB texture
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGB,
        width,
        height,
        0,
        gl.RGB,
        gl.UNSIGNED_BYTE,
        imageData,
      );

      // Linear filtering for MSDF
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

      // Store texture in renderer
      renderer.text.atlasTexture = texture;

      console.log("MSDF atlas texture uploaded successfully");
    },
    _renderer_draw_msdf_glyph: (
      x: number,
      y: number,
      width: number,
      height: number,
      u0: number,
      v0: number,
      u1: number,
      v1: number,
      r: number,
      g: number,
      b: number,
      a: number,
      fontSize: number,
      distanceRange: number,
    ): void => {
      if (!renderer.text.atlasTexture) {
        console.warn("MSDF atlas texture not uploaded yet");
        return;
      }

      const dpr = window.devicePixelRatio || 1;

      // Use text shader
      gl.useProgram(renderer.text.program);

      // Set uniforms
      gl.uniform2f(
        renderer.text.uniforms.resolution,
        canvas.width,
        canvas.height,
      );
      gl.uniform4f(
        renderer.text.uniforms.rect,
        x * dpr,
        y * dpr,
        width * dpr,
        height * dpr,
      );
      gl.uniform4f(renderer.text.uniforms.color, r, g, b, a);
      gl.uniform1f(renderer.text.uniforms.distanceRange, distanceRange);
      gl.uniform1f(renderer.text.uniforms.fontSize, fontSize * dpr);
      gl.uniform4f(renderer.text.uniforms.uvBounds, u0, v0, u1, v1);

      // Bind atlas texture
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, renderer.text.atlasTexture);
      gl.uniform1i(renderer.text.uniforms.texture, 0);

      // Draw quad (uses standard 0-1 normalized quad)
      gl.bindVertexArray(renderer.text.vao);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.bindVertexArray(null);
    },
  };
}
