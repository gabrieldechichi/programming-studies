class Constants {
  static readonly Float32Size = Float32Array.BYTES_PER_ELEMENT;
}

async function loadImage(
  src: string,
  allowCors?: boolean,
): Promise<HTMLImageElement> {
  return new Promise((resolve) => {
    const image = new Image();
    if (allowCors) {
      image.crossOrigin = "anonymous";
    }
    image.addEventListener("load", () => resolve(image));
    image.src = src;
  });
}

enum TexFormat {
  RGB = WebGL2RenderingContext.RGB,
  RGBA = WebGL2RenderingContext.RGBA,
}

enum TexFiltering {
  Nearest = WebGL2RenderingContext.NEAREST,
  Linear = WebGL2RenderingContext.LINEAR,
}

type CreateTexParamsBase = {
  gl: WebGL2RenderingContext;
  texIndex: number;

  skipFlipY?: boolean;
  format: TexFormat;
  minFilter: TexFiltering;
  magFilter: TexFiltering;
};

type CreateTexFromImgParams = CreateTexParamsBase & {
  image: HTMLImageElement;
};
type CreateTexFromPixelsParams = CreateTexParamsBase & {
  //todo: other array types
  pixels: Uint8Array;
  width: number;
  height: number;
};

class graphics {
  static async createAndBindTexture(
    params: CreateTexFromImgParams | CreateTexFromPixelsParams,
  ) {
    const { gl, texIndex, skipFlipY, format, minFilter, magFilter } = params;
    gl.activeTexture(gl.TEXTURE0 + texIndex);
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, skipFlipY || true);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, magFilter);

    if ("image" in params) {
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        format,
        params.image.width,
        params.image.height,
        0,
        format,
        gl.UNSIGNED_BYTE,
        params.image,
      );
    } else if ("pixels" in params) {
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        format,
        params.width,
        params.height,
        0,
        format,
        gl.UNSIGNED_BYTE,
        params.pixels,
      );
    }
  }
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
  in vec2 aTexCoord;
  in vec3 aColor;

  out vec3 vColor;
  out vec2 vTexCoord;

  void main() {
      vColor = aColor;
      vTexCoord = aTexCoord;
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
in vec2 vTexCoord;

uniform sampler2D uSampler;
uniform sampler2D uSamplerCat;

void main() {
    // vec3 color = vColor;
    vec3 color = vec3(1.);
    vec3 tex1 = texture(uSampler, vTexCoord).rgb;
    vec3 tex2 = texture(uSamplerCat, vTexCoord).rgb;
    color *= tex1;
    color *= tex2;
    fragColor = vec4(color, 1.0);
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
      //pos          //uv      //color
      -0.5, -0.5,    0,0,      1, 0, 0, 
      +0.5, -0.5,    1,0,      0, 1, 0,
      +0.5, +0.5,    1,1,      0, 0, 1,  
      +0.5, +0.5,    1,1,      0, 0, 1,  
      -0.5, +0.5,    0,1,      0, 0, 1,  
      -0.5, -0.5,    0,0,      1, 0, 0, 
  ])

  //number of floats per vertex
  const strideFloatCount = 7;
  const strideBytes = strideFloatCount * Constants.Float32Size;

  const vertexBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, vertexBufferData, gl.STATIC_DRAW);

  const aPosition = gl.getAttribLocation(program, "aPosition");
  const aTexCoord = gl.getAttribLocation(program, "aTexCoord");
  const aColor = gl.getAttribLocation(program, "aColor");
  gl.enableVertexAttribArray(aPosition);
  gl.enableVertexAttribArray(aTexCoord);
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
    aTexCoord,
    2,
    gl.FLOAT,
    false,
    strideBytes,
    2 * Constants.Float32Size,
  );

  gl.vertexAttribPointer(
    aColor,
    3,
    gl.FLOAT,
    false,
    strideBytes,
    (2 + 2) * Constants.Float32Size,
  );

  const uSampler = gl.getUniformLocation(program, "uSampler");
  gl.uniform1i(uSampler, 0);

  const uSamplerCat = gl.getUniformLocation(program, "uSamplerCat");
  gl.uniform1i(uSamplerCat, 1);

  const pixels = new Uint8Array([
    255, 255, 255, 230, 25, 75, 60, 180, 75, 255, 225, 25, 67, 99, 216, 245,
    130, 49, 145, 30, 180, 70, 240, 240, 240, 50, 230, 188, 246, 12, 250, 190,
    190, 0, 128, 128, 230, 190, 255, 154, 99, 36, 255, 250, 200, 0, 0, 0,
  ]);

  // bind texture 0
  graphics.createAndBindTexture({
    gl,
    texIndex: 0,
    format: TexFormat.RGB,
    minFilter: TexFiltering.Nearest,
    magFilter: TexFiltering.Nearest,
    pixels,
    width: 4,
    height: 4,
  });

  const cat = await loadImage(
    "https://t4.ftcdn.net/jpg/00/97/58/97/360_F_97589769_t45CqXyzjz0KXwoBZT9PRaWGHRk5hQqQ.jpg",
    true,
  );

  graphics.createAndBindTexture({
    gl,
    texIndex: 1,
    format: TexFormat.RGBA,
    minFilter: TexFiltering.Linear,
    magFilter: TexFiltering.Linear,
    image: cat,
  });

  const uScale = gl.getUniformLocation(program, "uScale");
  gl.uniform1f(uScale, 1.5);

  gl.drawArrays(gl.TRIANGLES, 0, 6);
}

main()
  .then(() => console.log("done"))
  .catch((error) => console.error(error));
