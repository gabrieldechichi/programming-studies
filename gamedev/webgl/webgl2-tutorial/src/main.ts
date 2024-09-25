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

  uniform float uPointSize;
  uniform vec2 uPosition;

  void main() {
      gl_PointSize = uPointSize;
      gl_Position = vec4(uPosition, 0.0, 1.0);
  }
  `;
  const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
  gl.shaderSource(vertexShader, vertexSource);
  gl.compileShader(vertexShader);
  gl.attachShader(program, vertexShader);

  const fragSource = `#version 300 es
precision mediump float;

out vec4 fragColor;

uniform vec3 uColor;

void main() {
    fragColor = vec4(uColor, 1.0);
}
  `
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

  const uPointSize = gl.getUniformLocation(program, "uPointSize");
  const uPosition = gl.getUniformLocation(program, "uPosition");
  const uColor = gl.getUniformLocation(program, "uColor");
  gl.uniform1f(uPointSize, 10);
  gl.uniform2f(uPosition, 0, -0.2);
  gl.uniform3f(uColor, 1, 0, 0);

  gl.drawArrays(gl.POINTS, 0, 1);

  gl.uniform3f(uColor, 0, 1, 0);
  gl.uniform1f(uPointSize, 50);
  gl.uniform2f(uPosition, 0, 0.3);

  gl.drawArrays(gl.POINTS, 0, 1);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
