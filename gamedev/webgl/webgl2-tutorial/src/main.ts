class Constants {
  static readonly Float32Size = Float32Array.BYTES_PER_ELEMENT;
}

async function loadFile(path: string): Promise<string> {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`Failed to load file: ${response.statusText}`);
  }
  return await response.text();
}

async function main() {
  const canvas = document.getElementById("canvas") as HTMLCanvasElement;
  const gl = canvas.getContext("webgl2");
  if (!gl) {
    return;
  }

  const program = gl.createProgram()!;

  const vertexSource = `#version 300 es

  uniform float uScale;
  in vec2 aPosition;
  in vec3 aColor;

  out vec3 vColor;

  void main() {
      vColor = aColor;
      gl_Position = vec4(aPosition * uScale, 0.0, 1.0);
  }
  `;
  const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
  gl.shaderSource(vertexShader, vertexSource);
  gl.compileShader(vertexShader);
  gl.attachShader(program, vertexShader);

  const fragSource = `#version 300 es
precision mediump float;

out vec4 fragColor;

in vec3 vColor;

void main() {
    fragColor = vec4(vColor, 1.0);
}
  `;
  const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER)!;
  gl.shaderSource(fragmentShader, fragSource);
  gl.compileShader(fragmentShader);
  gl.attachShader(program, fragmentShader);

  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.log(gl.getShaderInfoLog(vertexShader));
    console.log(gl.getShaderInfoLog(fragmentShader));
  }

  gl.useProgram(program);

  // prettier-ignore
  const vertexBufferData = new Float32Array([
      //pos    //color
      -0.5, -0.5,    1, 0, 0,
      +0.5, -0.5,    0, 1, 0,
      +0.0, +0.5,    0, 0, 1,
  ])
  //number of floats per vertex
  const strideFloatCount = 5;
  const strideBytes = strideFloatCount * Constants.Float32Size;

  const vertexBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, vertexBufferData, gl.STATIC_DRAW);

  const aPosition = gl.getAttribLocation(program, "aPosition");
  const aColor = gl.getAttribLocation(program, "aColor");
  gl.enableVertexAttribArray(aPosition);
  gl.enableVertexAttribArray(aColor);

  gl.vertexAttribPointer(
    aPosition,
    2,
    gl.FLOAT,
    false,
    strideBytes,
    0 * Constants.Float32Size,
  );
  gl.vertexAttribPointer(
    aColor,
    3,
    gl.FLOAT,
    false,
    strideBytes,
    2 * Constants.Float32Size,
  );

  const uScale = gl.getUniformLocation(program, "uScale");
  gl.uniform1f(uScale, 1.5);

  gl.drawArrays(gl.TRIANGLES, 0, 3);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
