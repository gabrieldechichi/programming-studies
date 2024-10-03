import * as constants from "src/engine/constants";
import * as graphics from "src/engine/graphics";
import { mat4, quat, vec2, vec3, vec4 } from "gl-matrix";

export enum Pivot {
  CENTER,
  TOP_LEFT,
  BOTTOM_LEFT,
}

const vertexSource = `#version 300 es
  precision mediump float;

  layout(location=0) in vec2 aVertexPos;
  layout(location=1) in vec4 aTexCoord;
  layout(location=2) in vec4 aColor;
  layout(location=3) in mat4 aModelMatrix;

  uniform mat4 uViewProjectionMatrix;

  out vec2 vTexCoord;
  out vec4 vColor;

  void main() {
      vec4 pos = uViewProjectionMatrix * aModelMatrix * vec4(aVertexPos, 0.0, 1.0);
      pos = uViewProjectionMatrix * aModelMatrix * vec4(aVertexPos, 0.0, 1.0);

      gl_Position = pos;

      vTexCoord = (aVertexPos * vec2(0.5, -0.5) + 0.5) * aTexCoord.zw + aTexCoord.xy ;
      vColor = aColor;
  }
`;

const fragSource = `#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;

uniform sampler2D uSampler;

void main() {
    vec4 color = texture(uSampler, vTexCoord);
    if (color.a < 0.1){discard;}
    color *= vColor;
    // if (color.a < 0.1){fragColor = vec4(1,0,0,1); return;}
    fragColor = color;
}
`;

const quad = new Float32Array([-1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1]);

class SpriteInstanceData {
  uvOffset: vec2;
  uvSize: vec2;
  color: vec4;
  modelMatrix: mat4;

  static UV_OFFSET_FLOAT_COUNT = 2;
  static UV_OFFSET_FLOAT_OFFSET = 0;
  static UV_OFFSET_BYTE_OFFSET =
    this.UV_OFFSET_FLOAT_OFFSET * constants.FLOAT_SIZE;

  static UV_SIZE_FLOAT_COUNT = 2;
  static UV_SIZE_FLOAT_OFFSET =
    this.UV_OFFSET_FLOAT_OFFSET + this.UV_OFFSET_FLOAT_COUNT;
  static UV_SIZE_BYTE_OFFSET = this.UV_SIZE_FLOAT_OFFSET * constants.FLOAT_SIZE;

  static COLOR_FLOAT_COUNT = 4;
  static COLOR_FLOAT_OFFSET =
    this.UV_SIZE_FLOAT_OFFSET + this.UV_SIZE_FLOAT_COUNT;
  static COLOR_BYTE_OFFSET = this.COLOR_FLOAT_OFFSET * constants.FLOAT_SIZE;

  static MODEL_MATRIX_FLOAT_COUNT = 16;
  static MODEL_MATRIX_FLOAT_OFFSET =
    this.COLOR_FLOAT_OFFSET + this.COLOR_FLOAT_COUNT;
  static MODEL_MATRIX_BYTE_OFFSET =
    this.MODEL_MATRIX_FLOAT_OFFSET * constants.FLOAT_SIZE;

  static STRIDE_FLOAT =
    this.MODEL_MATRIX_FLOAT_COUNT +
    this.UV_OFFSET_FLOAT_COUNT +
    this.UV_SIZE_FLOAT_COUNT +
    this.COLOR_FLOAT_COUNT;

  static STRIDE_BYTES = this.STRIDE_FLOAT * constants.FLOAT_SIZE;

  writeToFloat32Array(arr: Float32Array, offset: number = 0) {
    arr.set(
      this.modelMatrix,
      offset + SpriteInstanceData.MODEL_MATRIX_FLOAT_OFFSET,
    );
    arr.set(this.uvOffset, offset + SpriteInstanceData.UV_OFFSET_FLOAT_OFFSET);
    arr.set(this.uvSize, offset + SpriteInstanceData.UV_SIZE_FLOAT_OFFSET);
    arr.set(this.color, offset + SpriteInstanceData.COLOR_FLOAT_OFFSET);
  }
}

type SpriteRenderInstance = {
  texture: WebGLTexture;
  texWidth: number;
  texHeight: number;
  textureIndex: GLint;
  buffer: WebGLBuffer;
  instanceData: Float32Array;

  vertexBuffer: WebGLBuffer;
  vao: WebGLVertexArrayObject;

  instanceCountThisFrame: number;
};

//todo: add support for multiple buffers per atlas
export class SpriteRenderer {
  gl: WebGL2RenderingContext;
  program: WebGLProgram;
  renderInstances: SpriteRenderInstance[];

  aVertexPos: GLint;
  aModelMatrix: GLint;
  aTexCoord: GLint;
  aColor: GLint;
  uSampler: WebGLUniformLocation;
  uViewProjectionMatrix: WebGLUniformLocation;
  batchSize: number;

  static new(gl: WebGL2RenderingContext, batchSize: number) {
    const program = gl.createProgram()!;

    const vertexShader = gl.createShader(gl.VERTEX_SHADER)!;
    gl.shaderSource(vertexShader, vertexSource);
    gl.compileShader(vertexShader);
    gl.attachShader(program, vertexShader);

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

    const aVertexPos = gl.getAttribLocation(program, "aVertexPos");
    const aModelMatrix = gl.getAttribLocation(program, "aModelMatrix");
    const aTexCoord = gl.getAttribLocation(program, "aTexCoord");
    const aColor = gl.getAttribLocation(program, "aColor");
    const uSampler = gl.getUniformLocation(program, "uSampler");
    const uViewProjectionMatrix = gl.getUniformLocation(
      program,
      "uViewProjectionMatrix",
    );

    const spriteRenderer = new SpriteRenderer();
    spriteRenderer.gl = gl;

    spriteRenderer.program = program;
    spriteRenderer.renderInstances = [];
    spriteRenderer.aVertexPos = aVertexPos;
    spriteRenderer.aModelMatrix = aModelMatrix;
    spriteRenderer.aTexCoord = aTexCoord;
    spriteRenderer.aColor = aColor;
    spriteRenderer.uSampler = uSampler!;
    spriteRenderer.uViewProjectionMatrix = uViewProjectionMatrix!;
    spriteRenderer.batchSize = batchSize;

    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    return spriteRenderer;
  }

  async addAtlas(texturePath: string) {
    const { gl, renderInstances } = this;
    const tex = await graphics.loadImage(texturePath);

    //TODO: what if we go above the limit?
    const nextIndex = renderInstances.length;
    const webGLTex = graphics.createAndBindTexture({
      gl,
      texIndex: nextIndex,
      format: graphics.TexFormat.RGBA,
      minFilter: graphics.TexFiltering.Nearest,
      magFilter: graphics.TexFiltering.Nearest,
      generateMipMaps: true,
      image: tex,
    });

    this.addInstanceBatch(webGLTex!, tex.width, tex.height, nextIndex);

    return webGLTex;
  }

  addTexturePixels(pixels: Uint8Array, width: number, height: number) {
    const { gl, renderInstances } = this;

    //TODO: what if we go above the limit?
    const nextIndex = renderInstances.length;
    const webGLTex = graphics.createAndBindTexture({
      gl,
      texIndex: nextIndex,
      format: graphics.TexFormat.RGBA,
      minFilter: graphics.TexFiltering.Nearest,
      magFilter: graphics.TexFiltering.Nearest,
      generateMipMaps: false,
      pixels,
      width,
      height,
    });

    this.addInstanceBatch(webGLTex!, width, height, nextIndex);

    return webGLTex;
  }

  addInstanceBatch(
    webGLTex: WebGLTexture,
    width: number,
    height: number,
    nextIndex: number,
  ) {
    const { gl, renderInstances, aVertexPos, aModelMatrix, aTexCoord, aColor } =
      this;

    //initialize buffers
    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);

    //setup vertex buffer
    gl.enableVertexAttribArray(aVertexPos);

    const vertexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.vertexAttribPointer(
      aVertexPos,
      2,
      gl.FLOAT,
      false,
      2 * constants.FLOAT_SIZE,
      0 * constants.FLOAT_SIZE,
    );

    //setup instance buffer
    const instanceData = new Float32Array(
      this.batchSize * SpriteInstanceData.STRIDE_FLOAT,
    );

    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, instanceData.byteLength, gl.DYNAMIC_DRAW);

    gl.enableVertexAttribArray(aTexCoord);
    gl.enableVertexAttribArray(aColor);
    gl.vertexAttribDivisor(aTexCoord, 1);
    gl.vertexAttribDivisor(aColor, 1);

    gl.vertexAttribPointer(
      aTexCoord,
      SpriteInstanceData.UV_OFFSET_FLOAT_COUNT +
        SpriteInstanceData.UV_SIZE_FLOAT_COUNT,
      gl.FLOAT,
      false,
      SpriteInstanceData.STRIDE_BYTES,
      SpriteInstanceData.UV_OFFSET_BYTE_OFFSET,
    );

    gl.vertexAttribPointer(
      aColor,
      SpriteInstanceData.COLOR_FLOAT_COUNT,
      gl.FLOAT,
      false,
      SpriteInstanceData.STRIDE_BYTES,
      SpriteInstanceData.COLOR_BYTE_OFFSET,
    );

    for (let i = 0; i < 4; i++) {
      // Enable vertex attribute
      gl.enableVertexAttribArray(aModelMatrix + i);

      // Set the vertex attribute parameters
      gl.vertexAttribPointer(
        aModelMatrix + i,
        4, //4 foats
        gl.FLOAT,
        false,
        SpriteInstanceData.STRIDE_BYTES,
        (SpriteInstanceData.MODEL_MATRIX_FLOAT_OFFSET + i * 4) * constants.FLOAT_SIZE,
      );

      // For instanced rendering
      gl.vertexAttribDivisor(aModelMatrix + i, 1);
    }

    gl.bindVertexArray(null);

    const renderInstance = {
      texture: webGLTex,
      texWidth: width,
      texHeight: height,
      textureIndex: nextIndex,
      buffer,
      instanceData,
      vao,
      vertexBuffer,
      instanceCountThisFrame: 0,
    } as SpriteRenderInstance;

    renderInstances.push(renderInstance);
  }

  drawSprite({
    sprite,
    pos,
    scale,
    color,
    pivot,
  }: {
    sprite: graphics.Sprite;
    pos?: vec3;
    scale?: vec2;
    color?: vec4;
    pivot?: Pivot;
  }) {
    const { renderInstances } = this;
    const index = renderInstances.findIndex(
      (i) => i.texture === sprite.texture,
    );
    if (index < 0) {
      throw new Error(`Texture atlas not setup for texture: ${sprite.texture}`);
    }

    const renderInstance = renderInstances[index];

    const floatOffset =
      renderInstance.instanceCountThisFrame * SpriteInstanceData.STRIDE_FLOAT;

    const { offset, size } = graphics.getUvOffsetAndScale(
      sprite,
      renderInstance.texWidth,
      renderInstance.texHeight,
    );

    pos ||= [0, 0, 0];
    scale = scale || [1, 1];
    pivot ||= Pivot.CENTER;

    switch (pivot) {
      case Pivot.CENTER:
        break;
      case Pivot.TOP_LEFT:
        pos[0] += scale[0];
        pos[1] -= scale[1];
        break;
      case Pivot.BOTTOM_LEFT:
        pos[0] += scale[0];
        pos[1] += scale[1];
        break;
    }

    const modelMatrix = mat4.create();
    const rotQuat = quat.fromEuler(quat.create(), 0, 0, 0);
    mat4.fromRotationTranslationScale(modelMatrix, rotQuat, pos, [
      scale[0],
      scale[1],
      1,
    ]);

    const instanceData = new SpriteInstanceData();
    instanceData.modelMatrix = modelMatrix;
    instanceData.uvOffset = offset;
    instanceData.uvSize = size;
    instanceData.color = color || [1, 1, 1, 1];

    instanceData.writeToFloat32Array(renderInstance.instanceData, floatOffset);
    //todo: guard against MAX_INSTANCE
    renderInstance.instanceCountThisFrame++;
  }

  drawText({
    text,
    topLeft,
    font,
    size,
  }: {
    text: string;
    topLeft: vec2;
    font: graphics.Font;
    size: number;
  }) {
    size = size || font.size;
    const scale = size / font.size;
    let baseLine = topLeft[1] - font.lineHeight * scale;

    const lines = text.split("\n");
    for (const line of lines) {
      let cursor = 0;
      for (var i = 0; i < line.length; i++) {
        const c = line[i].charCodeAt(0);
        const fontChar = font.characters[c];
        const sprite = graphics.charToSprite(c, font);

        const pos: vec3 = [
          topLeft[0] + cursor + fontChar.offset[0] * scale,
          baseLine - (fontChar.offset[1] + sprite.h) * scale,
          0.7,
        ];

        this.drawSprite({
          sprite,
          pos,
          scale: [sprite.w * scale, sprite.h * scale],
          pivot: Pivot.BOTTOM_LEFT,
        });

        cursor += (fontChar.advance + sprite.w) * scale;
      }

      baseLine -= font.lineHeight * 2 * scale;
    }
  }

  render(viewProjectionMatrix: mat4) {
    const { renderInstances, gl, uSampler, uViewProjectionMatrix } = this;

    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    gl.uniformMatrix4fv(uViewProjectionMatrix, false, viewProjectionMatrix);

    for (const instanceBatch of renderInstances) {
      const {
        vao,
        buffer,
        textureIndex,
        instanceData,
        instanceCountThisFrame,
      } = instanceBatch;
      if (instanceBatch.instanceCountThisFrame <= 0) {
        continue;
      }

      gl.bindVertexArray(vao);
      gl.uniform1i(uSampler, textureIndex);
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      // gl.bufferSubData(
      //   gl.ARRAY_BUFFER,
      //   0,
      //   instanceData,
      //   0,
      //   instanceCountThisFrame * SpriteInstanceData.STRIDE_BYTES,
      // );

      gl.bufferData(gl.ARRAY_BUFFER, instanceData, gl.DYNAMIC_DRAW);

      gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, instanceCountThisFrame);
      gl.bindVertexArray(null);
    }
  }

  endFrame() {
    for (const instanceBatch of this.renderInstances) {
      instanceBatch.instanceCountThisFrame = 0;
    }
  }

  destroy() {
    for (const i of this.renderInstances) {
      this.gl.deleteBuffer(i.buffer);
      this.gl.deleteBuffer(i.vertexBuffer);
      this.gl.deleteTexture(i.texture);
    }
    this.gl.deleteProgram(this.program);
  }
}
