import * as constants from "./constants";
import * as graphics from "./graphics";

async function main() {
  const canvas = document.getElementById("canvas") as HTMLCanvasElement;
  const gl = canvas.getContext("webgl2");
  if (!gl) {
    return;
  }

  const program = gl.createProgram()!;

  const vertexSource = `#version 300 es

  in vec2 aPosition;
  in vec2 aTexCoord;

  out vec2 vTexCoord;

  void main() {
      vTexCoord = aTexCoord;
      gl_Position = vec4(aPosition, 0.0, 1.0);
  }
  `;

  const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
  gl.shaderSource(vertexShader, vertexSource);
  gl.compileShader(vertexShader);
  gl.attachShader(program, vertexShader);

  const fragSource = `#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSampler;

void main() {
    fragColor = texture(uSampler, vTexCoord);
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
  const aTexCoord = gl.getAttribLocation(program, "aTexCoord");
  const uSampler = gl.getUniformLocation(program, "uSampler");

  const quad = [-1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1];
  const positionData = new Float32Array([
    // Quad 1
    ...quad.map((v, i) => v / 2 + (i % 2 === 0 ? -0.5 : 0.5)),
    ...quad.map((v, i) => v / 2 + (i % 2 === 0 ? 0.5 : 0.5)),
    ...quad.map((v, i) => v / 2 + (i % 2 === 0 ? -0.5 : -0.5)),
    ...quad.map((v, i) => v / 2 + (i % 2 === 0 ? 0.5 : -0.5)),
  ]);

  const quadCount = 4;
  const texCoordData = new Float32Array(2 * quadCount * 6);

  const positionBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, positionData, gl.STATIC_DRAW);

  gl.enableVertexAttribArray(aPosition);
  gl.vertexAttribPointer(
    aPosition,
    2,
    gl.FLOAT,
    false,
    2 * constants.FLOAT_SIZE,
    0,
  );

  const texCoordBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);

  gl.enableVertexAttribArray(aTexCoord);
  gl.vertexAttribPointer(
    aTexCoord,
    2,
    gl.FLOAT,
    false,
    2 * constants.FLOAT_SIZE,
    0,
  );

  const atlasData = await graphics.loadAtlas("./assets/textures/atlas.json");
  const atlas = await graphics.loadImage("./assets/textures/atlas.png");

  texCoordData.set(
    graphics.getUvsForQuad6(
      atlasData["medievalTile_03"],
      atlas.width,
      atlas.height,
    ),
    0 * 12,
  );

  texCoordData.set(
    graphics.getUvsForQuad6(
      atlasData["medievalTile_17"],
      atlas.width,
      atlas.height,
    ),
    1 * 12,
  );

  texCoordData.set(
    graphics.getUvsForQuad6(
      atlasData["medievalTile_05"],
      atlas.width,
      atlas.height,
    ),
    2 * 12,
  );

  texCoordData.set(
    graphics.getUvsForQuad6(
      atlasData["medievalTile_07"],
      atlas.width,
      atlas.height,
    ),
    3 * 12,
  );

  gl.bufferData(gl.ARRAY_BUFFER, texCoordData, gl.DYNAMIC_DRAW);

  graphics.createAndBindTexture({
    gl,
    texIndex: 0,
    format: graphics.TexFormat.RGBA,
    minFilter: graphics.TexFiltering.Nearest,
    magFilter: graphics.TexFiltering.Nearest,
    generateMipMaps: true,
    image: atlas,
  });
  gl.uniform1i(uSampler, 0);

  gl.drawArrays(gl.TRIANGLES, 0, quadCount * 6);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
