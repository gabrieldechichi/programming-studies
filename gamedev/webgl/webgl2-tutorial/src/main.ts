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
  in vec2 aOffset;
  in float aScale;
  in vec4 aTexCoord;

  out vec2 vTexCoord;

  void main() {
      gl_Position = vec4(aPosition * aScale + aOffset, 0.0, 1.0);
      vTexCoord = (aPosition * 0.5 + 0.5) * aTexCoord.zw + aTexCoord.xy ;
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
    // fragColor = vec4(1,0,0,1);
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
  const aOffset = gl.getAttribLocation(program, "aOffset");
  const aScale = gl.getAttribLocation(program, "aScale");
  const aTexCoord = gl.getAttribLocation(program, "aTexCoord");
  const uSampler = gl.getUniformLocation(program, "uSampler");

  //define vertex data
  {
    const quad = new Float32Array([-1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1]);

    const vertexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(aPosition);
    gl.vertexAttribPointer(
      aPosition,
      2,
      gl.FLOAT,
      false,
      2 * constants.FLOAT_SIZE,
      0 * constants.FLOAT_SIZE,
    );
  }

  const instanceCount = 4;
  //define instance data (offset, scale)
  {
    //prettier-ignore
    const instanceData = new Float32Array([
      // pos       //scale
      -0.5, 0.5,   0.5,
      // pos       //scale
      0.5, 0.5,    0.5,
      // pos       //scale
      -0.5, -0.5,  0.5,
      // pos       //scale
      0.5, -0.5,   0.5,
    ]);

    const instanceDataStride = 3 * constants.FLOAT_SIZE;

    const instanceBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, instanceBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, instanceData, gl.STATIC_DRAW);

    gl.enableVertexAttribArray(aOffset);
    gl.enableVertexAttribArray(aScale);
    gl.vertexAttribDivisor(aOffset, 1);
    gl.vertexAttribDivisor(aScale, 1);
    gl.vertexAttribPointer(
      aOffset,
      2,
      gl.FLOAT,
      false,
      instanceDataStride,
      0 * constants.FLOAT_SIZE,
    );

    gl.vertexAttribPointer(
      aScale,
      1,
      gl.FLOAT,
      false,
      instanceDataStride,
      2 * constants.FLOAT_SIZE,
    );
  }

  //vertex (uv)
  //todo: merge with rest of vertex data
  {
    const texCoordData = new Float32Array(4 * instanceCount);

    const texCoordBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);

    gl.enableVertexAttribArray(aTexCoord);
    gl.vertexAttribDivisor(aTexCoord, 1);
    gl.vertexAttribPointer(
      aTexCoord,
      4,
      gl.FLOAT,
      false,
      4 * constants.FLOAT_SIZE,
      0,
    );

    const atlasData = await graphics.loadAtlas("./assets/textures/atlas.json");
    const atlas = await graphics.loadImage("./assets/textures/atlas.png");

    texCoordData.set(
      graphics.getUvOffsetAndScale(
        atlasData["medievalTile_03"],
        atlas.width,
        atlas.height,
      ),
      0 * 4,
    );

    texCoordData.set(
      graphics.getUvOffsetAndScale(
        atlasData["medievalTile_17"],
        atlas.width,
        atlas.height,
      ),
      1 * 4,
    );

    texCoordData.set(
      graphics.getUvOffsetAndScale(
        atlasData["medievalTile_05"],
        atlas.width,
        atlas.height,
      ),
      2 * 4,
    );

    texCoordData.set(
      graphics.getUvOffsetAndScale(
        atlasData["medievalTile_07"],
        atlas.width,
        atlas.height,
      ),
      3 * 4,
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
  }

  gl.uniform1i(uSampler, 0);

  gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, instanceCount);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
