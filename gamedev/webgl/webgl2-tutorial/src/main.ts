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

  const vertexSource = await loadFile("assets/shaders/vert.glsl");
  const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
  gl.shaderSource(vertexShader, vertexSource);
  gl.compileShader(vertexShader);
  gl.attachShader(program, vertexShader);

  const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER)!;
  const fragSource = await loadFile("assets/shaders/frag.glsl");
  gl.shaderSource(fragmentShader, fragSource);
  gl.compileShader(fragmentShader);
  gl.attachShader(program, fragmentShader);

  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.log(gl.getShaderInfoLog(vertexShader));
    console.log(gl.getShaderInfoLog(fragmentShader));
  }

  gl.useProgram(program);

  gl.drawArrays(gl.POINTS, 0, 1);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
