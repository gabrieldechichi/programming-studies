// Vertex shader - simple full-screen quad
const vertexShaderSource = `#version 300 es
in vec2 a_position;
in vec2 a_texCoord;
out vec2 v_texCoord;

void main() {
  gl_Position = vec4(a_position, 0.0, 1.0);
  v_texCoord = a_texCoord;
}
`;

// Fragment shader - sample the texture
const fragmentShaderSource = `#version 300 es
precision highp float;
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_texture;

void main() {
  fragColor = texture(u_texture, v_texCoord);
}
`;

export interface ShaderProgramInfo {
  program: WebGLProgram;
  attribs: {
    position: number;
    texCoord: number;
  };
  uniforms: {
    texture: WebGLUniformLocation | null;
  };
}

export interface QuadBuffers {
  position: WebGLBuffer;
  texCoord: WebGLBuffer;
  vao: WebGLVertexArrayObject;
}

function compileShader(gl: WebGL2RenderingContext, source: string, type: number): WebGLShader | null {
  const shader = gl.createShader(type);
  if (!shader) return null;

  gl.shaderSource(shader, source);
  gl.compileShader(shader);

  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    console.error('Shader compilation error:', gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
    return null;
  }

  return shader;
}

export function createShaderProgram(gl: WebGL2RenderingContext): ShaderProgramInfo | null {
  const vertexShader = compileShader(gl, vertexShaderSource, gl.VERTEX_SHADER);
  const fragmentShader = compileShader(gl, fragmentShaderSource, gl.FRAGMENT_SHADER);

  if (!vertexShader || !fragmentShader) {
    return null;
  }

  const program = gl.createProgram();
  if (!program) return null;

  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.error('Program linking error:', gl.getProgramInfoLog(program));
    gl.deleteProgram(program);
    return null;
  }

  return {
    program,
    attribs: {
      position: gl.getAttribLocation(program, 'a_position'),
      texCoord: gl.getAttribLocation(program, 'a_texCoord'),
    },
    uniforms: {
      texture: gl.getUniformLocation(program, 'u_texture'),
    },
  };
}

export function createQuadBuffers(gl: WebGL2RenderingContext, programInfo: ShaderProgramInfo): QuadBuffers | null {
  // Full-screen quad positions (-1 to 1)
  const positions = new Float32Array([
    -1, -1,
     1, -1,
    -1,  1,
     1,  1,
  ]);

  // Texture coordinates (0 to 1, flipped Y)
  const texCoords = new Float32Array([
    0, 1,
    1, 1,
    0, 0,
    1, 0,
  ]);

  const positionBuffer = gl.createBuffer();
  const texCoordBuffer = gl.createBuffer();
  const vao = gl.createVertexArray();

  if (!positionBuffer || !texCoordBuffer || !vao) {
    return null;
  }

  gl.bindVertexArray(vao);

  // Position attribute
  gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
  gl.enableVertexAttribArray(programInfo.attribs.position);
  gl.vertexAttribPointer(programInfo.attribs.position, 2, gl.FLOAT, false, 0, 0);

  // TexCoord attribute
  gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, texCoords, gl.STATIC_DRAW);
  gl.enableVertexAttribArray(programInfo.attribs.texCoord);
  gl.vertexAttribPointer(programInfo.attribs.texCoord, 2, gl.FLOAT, false, 0, 0);

  gl.bindVertexArray(null);

  return {
    position: positionBuffer,
    texCoord: texCoordBuffer,
    vao,
  };
}

export function renderQuad(
  gl: WebGL2RenderingContext,
  programInfo: ShaderProgramInfo,
  quadBuffers: QuadBuffers,
  texture: WebGLTexture
): void {
  gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
  gl.clearColor(0.1, 0.1, 0.1, 1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.useProgram(programInfo.program);

  // Bind texture
  gl.activeTexture(gl.TEXTURE0);
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.uniform1i(programInfo.uniforms.texture, 0);

  // Draw quad
  gl.bindVertexArray(quadBuffers.vao);
  gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
  gl.bindVertexArray(null);
}
