import * as graphics from "./graphics";
import * as constants from "./constants";

async function main() {
  const canvas = document.getElementById("canvas") as HTMLCanvasElement;
  const gl = canvas.getContext("webgl2");
  if (!gl) {
    return;
  }

  const program = gl.createProgram()!;

  const vertexSource = `#version 300 es

  in vec2 aPosition;
  in float aPointSize;
  in vec3 aColor;

  out vec3 vColor;

  void main() {
      vColor = aColor;
      gl_PointSize = aPointSize;
      gl_Position = vec4(aPosition, 0.0, 1.0);
  }
  `;

  const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
  gl.shaderSource(vertexShader, vertexSource);
  gl.compileShader(vertexShader);
  gl.attachShader(program, vertexShader);

  const fragSource = `#version 300 es
precision mediump float;

in vec3 vColor;
out vec4 fragColor;

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

  const aPosition = gl.getAttribLocation(program, "aPosition");
  const aPointSize = gl.getAttribLocation(program, "aPointSize");
  const aColor = gl.getAttribLocation(program, "aColor");

  //number of floats per vertex
  const strideFloatCount = 6;
  const strideBytes = strideFloatCount * constants.FLOAT_SIZE;

  let vao1: WebGLVertexArrayObject | null;
  //bind vao1
  {
    //prettier-ignore
    const data1 = new Float32Array([
      //pos         //color         //size
      -0.8, 0.6, 1, 0.75, 0.75, 125, -0.3, 0.6, 0, 0.75, 1, 32, 0.3, 0.6, 0.5,
      1, 0.75, 75, 0.8, 0.6, 0, 0.75, 0.75, 9,
    ]);

    const bufferData1 = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, bufferData1);
    gl.bufferData(gl.ARRAY_BUFFER, data1, gl.STATIC_DRAW);

    vao1 = gl.createVertexArray();
    gl.bindVertexArray(vao1);

    gl.enableVertexAttribArray(aPosition);
    gl.enableVertexAttribArray(aPointSize);
    gl.enableVertexAttribArray(aColor);

    gl.vertexAttribPointer(
      aPosition,
      2,
      gl.FLOAT,
      false,
      strideBytes,
      0 * constants.FLOAT_SIZE,
    );
    gl.vertexAttribPointer(
      aColor,
      3,
      gl.FLOAT,
      false,
      strideBytes,
      2 * constants.FLOAT_SIZE,
    );

    gl.vertexAttribPointer(
      aPointSize,
      1,
      gl.FLOAT,
      false,
      strideBytes,
      (2 + 3) * constants.FLOAT_SIZE,
    );

    gl.bindVertexArray(null);
  }

  let vao2: WebGLVertexArrayObject | null;
  {
    //prettier-ignore
    const data2 = new Float32Array([
      //position      //color        //size
      -0.8, -0.6,     0.25, 0, 0,    25,
      -0.3, -0.6,     0, 0, 0.25,    132,
      0.3, -0.6,      0, 0.25, 0,    105,
      0.6, -0.6,      0.25, 0, 0.25, 90,
    ]);

    const bufferData2 = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, bufferData2);
    gl.bufferData(gl.ARRAY_BUFFER, data2, gl.STATIC_DRAW);

    vao2 = gl.createVertexArray();
    gl.bindVertexArray(vao2);

    gl.enableVertexAttribArray(aPosition);
    gl.enableVertexAttribArray(aColor);
    gl.enableVertexAttribArray(aPointSize);

    gl.vertexAttribPointer(
      aPosition,
      2,
      gl.FLOAT,
      false,
      strideBytes,
      0 * constants.FLOAT_SIZE,
    );
    gl.vertexAttribPointer(
      aColor,
      3,
      gl.FLOAT,
      false,
      strideBytes,
      2 * constants.FLOAT_SIZE,
    );
    gl.vertexAttribPointer(
      aPointSize,
      1,
      gl.FLOAT,
      false,
      strideBytes,
      (2 + 3) * constants.FLOAT_SIZE,
    );

    gl.bindVertexArray(null);
  }

  gl.bindVertexArray(vao1);
  gl.drawArrays(gl.POINTS, 0, 6);

  gl.bindVertexArray(vao2);
  gl.drawArrays(gl.POINTS, 0, 6);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
