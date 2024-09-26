export enum TexFormat {
  RGB = WebGL2RenderingContext.RGB,
  RGBA = WebGL2RenderingContext.RGBA,
}

export enum TexFiltering {
  Nearest = WebGL2RenderingContext.NEAREST,
  Linear = WebGL2RenderingContext.LINEAR,
}

export type CreateTexParamsBase = {
  gl: WebGL2RenderingContext;
  texIndex: number;

  skipFlipY?: boolean;
  format: TexFormat;
  minFilter: TexFiltering;
  magFilter: TexFiltering;
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
