import * as core from "./core";

export enum TexFormat {
  RGB = WebGL2RenderingContext.RGB,
  RGBA = WebGL2RenderingContext.RGBA,
}

export enum TexFiltering {
  Nearest = WebGL2RenderingContext.NEAREST,
  Linear = WebGL2RenderingContext.LINEAR,
}

export type SpriteRegion = {
  x: number;
  y: number;
  w: number;
  h: number;
};

export type Sprite = SpriteRegion & { texture: WebGLTexture };

export async function loadAtlas(
  path: string,
): Promise<Record<string, SpriteRegion>> {
  const atlasStr = await core.loadFile(path);
  return JSON.parse(atlasStr) as Record<string, SpriteRegion>;
}

export function getUvsForQuad6(
  sprite: SpriteRegion,
  texWidth: number,
  texHeight: number,
  padding?: number
): number[] {
  const u = sprite.x / texWidth;
  const v = sprite.y / texHeight;
  const w = sprite.w / texWidth;
  const h = sprite.h / texHeight;

  padding ||= 0.0
  const hPadding = padding / texWidth;
  const vPadding = padding / texHeight;
  
  console.log(sprite, texWidth, texHeight);
  //prettier-ignore
  return [
    u + hPadding,       v + h - vPadding,
    u + w - hPadding,   v + h - vPadding,
    u + w - hPadding,   v + vPadding,
    u + w - hPadding,   v + vPadding,
    u + hPadding,       v + vPadding,
    u + hPadding,       v + h - vPadding,
  ];
}

export type CreateTexParamsBase = {
  gl: WebGL2RenderingContext;
  texIndex: number;

  flipY?: boolean;
  format: TexFormat;
  minFilter: TexFiltering;
  magFilter: TexFiltering;
  generateMipMaps?: boolean;
};

export type CreateTexFromImgParams = CreateTexParamsBase & {
  image: HTMLImageElement;
};

export type CreateTexFromPixelsParams = CreateTexParamsBase & {
  //todo: other array types
  pixels: Uint8Array;
  width: number;
  height: number;
};

export async function createAndBindTexture(
  params: CreateTexFromImgParams | CreateTexFromPixelsParams,
) {
  const { gl, texIndex, flipY, format, minFilter, magFilter, generateMipMaps } =
    params;
  gl.activeTexture(gl.TEXTURE0 + texIndex);
  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flipY || false);

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

  if (generateMipMaps) {
    gl.generateMipmap(gl.TEXTURE_2D);
  }
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, minFilter);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, magFilter);
}

export async function loadImage(
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
