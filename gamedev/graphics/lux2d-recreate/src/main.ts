function initWebGL(canvas: HTMLCanvasElement): WebGLRenderingContext | null {
  const gl = canvas.getContext("webgl") as WebGLRenderingContext;
  if (!gl) {
    console.error(
      "Unable to initialize WebGL. Your browser may not support it.",
    );
    return null;
  }
  return gl;
}

async function loadShaderFromFile(
  gl: WebGLRenderingContext,
  type: number,
  url: string,
): Promise<WebGLShader | null> {
  const r = await fetch(url);
  if (!r.ok) {
    console.error(`Failed to loader shader ${url}`);
    return null;
  }
  const source = await r.text();
  return loadShader(gl, type, source);
}

function loadShader(
  gl: WebGLRenderingContext,
  type: number,
  source: string,
): WebGLShader | null {
  const shader = gl.createShader(type);
  if (!shader) {
    console.error("Unable to create shader");
    return null;
  }
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    console.error(
      `An error occurred compiling the shaders: ${gl.getShaderInfoLog(shader)}`,
    );
    gl.deleteShader(shader);
    return null;
  }
  return shader;
}

type Pipeline = NonNullable<ReturnType<typeof createPipeline>>;

function createPipeline(
  gl: WebGLRenderingContext,
  vertexShader: WebGLShader,
  fragmentShader: WebGLShader,
) {
  const shaderProgram = gl.createProgram()!;

  gl.attachShader(shaderProgram, vertexShader);
  gl.attachShader(shaderProgram, fragmentShader);
  gl.linkProgram(shaderProgram);
  if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
    console.error(
      `Unable to initialize the shader program: ${gl.getProgramInfoLog(shaderProgram)}`,
    );
    return null;
  }

  const program = {
    program: shaderProgram,
    attribLocations: {
      vertexPosition: gl.getAttribLocation(shaderProgram, "aVertexPosition"),
      vertexColor: gl.getAttribLocation(shaderProgram, "aVertexColor"),
    },
  };

  console.log(gl.getAttribLocation(shaderProgram, "aVertexPosition"));
  console.log(gl.getAttribLocation(shaderProgram, "aVertexColor"));

  if (
    program.attribLocations.vertexPosition === -1 ||
    program.attribLocations.vertexColor === -1
  ) {
    console.error("Failed to get attribute location");
    return null;
  }

  const positionBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
  const positions = [0.0, 1.0, -1.0, -1.0, 1.0, -1.0];
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

  const colorBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
  const colors = [
    1.0,
    0.0,
    0.0, // Red
    0.0,
    1.0,
    0.0, // Green
    0.0,
    0.0,
    1.0, // Blue
  ];
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);

  const buffers = {
    position: positionBuffer,
    colors: colorBuffer,
  };

  return {
    program,
    buffers,
  };
}

function drawScene(gl: WebGLRenderingContext, { program, buffers }: Pipeline) {
  gl.clearColor(0.0, 0.0, 0.0, 1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);

  // Position buffer
  gl.bindBuffer(gl.ARRAY_BUFFER, buffers.position);
  gl.vertexAttribPointer(
    program.attribLocations.vertexPosition,
    2,
    gl.FLOAT,
    false,
    0,
    0,
  );
  gl.enableVertexAttribArray(program.attribLocations.vertexPosition);

  // Color buffer
  gl.bindBuffer(gl.ARRAY_BUFFER, buffers.colors);
  gl.vertexAttribPointer(
    program.attribLocations.vertexColor,
    3,
    gl.FLOAT,
    false,
    0,
    0,
  );
  gl.enableVertexAttribArray(program.attribLocations.vertexColor);

  gl.useProgram(program.program);
  gl.drawArrays(gl.TRIANGLES, 0, 3);
}

async function main() {
  const canvas = document.getElementById("glCanvas") as HTMLCanvasElement;
  const gl = initWebGL(canvas);
  if (!gl) {
    return;
  }

  const vertexShader = await loadShaderFromFile(
    gl,
    gl.VERTEX_SHADER,
    "/resources/shaders/triangleV.glsl",
  )!;
  const fragmentShader = await loadShaderFromFile(
    gl,
    gl.FRAGMENT_SHADER,
    "/resources/shaders/triangleF.glsl",
  )!;
  const pipeline = createPipeline(gl, vertexShader!, fragmentShader!);
  if (pipeline) {
    drawScene(gl, pipeline);
  }
}

window.onload = main;
