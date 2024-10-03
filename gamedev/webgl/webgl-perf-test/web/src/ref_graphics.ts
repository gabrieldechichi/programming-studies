import { vec2 } from "gl-matrix";
import * as core from "src/engine/core";

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
export class SpriteAtlas {
  texture: WebGLTexture;
  sprites: Record<string, SpriteRegion>;

  get(id: string) {
    return { texture: this.texture, ...this.sprites[id] } as Sprite;
  }
}

export async function loadAtlas(
  path: string,
): Promise<Record<string, SpriteRegion>> {
  const atlasStr = await core.loadFile(path);
  return JSON.parse(atlasStr) as Record<string, SpriteRegion>;
}

export async function loadAtlasWithTexture(
  texture: WebGLTexture,
  path: string,
): Promise<SpriteAtlas> {
  const atlasStr = await core.loadFile(path);
  const atlas = new SpriteAtlas();
  atlas.texture = texture;
  atlas.sprites = JSON.parse(atlasStr) as Record<string, SpriteRegion>;

  return atlas;
}

export type SpriteAnimationData = {
  texture: WebGLTexture;
  frames: SpriteRegion[];
};

export class SpritesheetAnimations {
  texture: WebGLTexture;
  animations: Record<string, SpriteRegion[]>;

  get(name: string): SpriteAnimationData {
    return {
      frames: this.animations[name],
      texture: this.texture,
    } as SpriteAnimationData;
  }
}

export class SpriteAnimationState {
  animation: SpriteAnimationData;
  time: number;
  fps: number;
  frameTime: number;
  currentIndex: number;

  constructor(animation: SpriteAnimationData, fps: number) {
    this.animation = animation;
    this.time = 0;
    this.setFps(fps);
    this.currentIndex = 0;
  }

  update(dt: number) {
    this.time += dt;
    if (this.time >= this.frameTime) {
      this.currentIndex =
        (this.currentIndex + 1) % this.animation.frames.length;
      this.time = 0;
    }
  }

  setFps(fps: number) {
    this.fps = fps;
    this.frameTime = 1 / fps;
  }

  currentFrame() {
    return {
      ...this.animation.frames[this.currentIndex],
      texture: this.animation.texture,
    } as Sprite;
  }
}

export function toSprite(texture: WebGLTexture, spriteRegion: SpriteRegion) {
  return { ...spriteRegion, texture } as Sprite;
}

export async function loadSpritesheetAnimations(
  texture: WebGLTexture,
  path: string,
) {
  const atlasStr = await core.loadFile(path);
  const spriteSheetAnimation = new SpritesheetAnimations();
  spriteSheetAnimation.texture = texture;
  spriteSheetAnimation.animations = JSON.parse(atlasStr);
  return spriteSheetAnimation;
}

export function getUvsForQuad6(
  sprite: SpriteRegion,
  texWidth: number,
  texHeight: number,
  padding?: number,
): number[] {
  const u = sprite.x / texWidth;
  const v = sprite.y / texHeight;
  const w = sprite.w / texWidth;
  const h = sprite.h / texHeight;

  padding ||= 0.0;
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

export function getUvOffsetAndScale(
  sprite: SpriteRegion,
  texWidth: number,
  texHeight: number,
  padding?: number,
): { offset: vec2; size: vec2 } {
  const u = sprite.x / texWidth;
  const v = sprite.y / texHeight;
  const w = sprite.w / texWidth;
  const h = sprite.h / texHeight;

  padding ||= 0.0;
  const hPadding = padding / texWidth;
  const vPadding = padding / texHeight;
  return {
    offset: [u + hPadding, v + vPadding],
    size: [w - hPadding, h - vPadding],
  };
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

export function createAndBindTexture(
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
  return texture;
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

export type FontChar = {
  id: number;
  xy: vec2;
  wh: vec2;
  offset: vec2;
  advance: number;
};

export type Font = {
  texture: WebGLTexture;
  lineHeight: number;
  size: number;
  characters: { [id: number]: FontChar };
};

export async function loadFontWithTexture(
  texture: WebGLTexture,
  xmlPath: string,
): Promise<Font> {
  const font = { characters: {} } as Font;
  font.texture = texture;

  const xmlTextReq = await fetch(xmlPath);
  const xmlText = await xmlTextReq.text();

  const xml = new DOMParser().parseFromString(xmlText, "text/xml");

  font.lineHeight = parseInt(
    xml.querySelector("common")?.getAttribute("lineHeight")!,
  );
  font.size = parseInt(xml.querySelector("info")?.getAttribute("size")!);

  xml.querySelectorAll("char").forEach((element) => {
    const id = parseInt(element.getAttribute("id")!);
    const x = parseInt(element.getAttribute("x")!);
    const y = parseInt(element.getAttribute("y")!);
    const width = parseInt(element.getAttribute("width")!);
    const height = parseInt(element.getAttribute("height")!);
    const xoffset = parseInt(element.getAttribute("xoffset")!);
    const yoffset = parseInt(element.getAttribute("yoffset")!);
    const xadvance = parseInt(element.getAttribute("xadvance")!);

    const fontChar: FontChar = {
      id,
      xy: [x, y],
      wh: [width, height],
      offset: [xoffset, yoffset],
      advance: xadvance,
    };

    font.characters[id] = fontChar;
  });
  return font;
}

export function charToSprite(c: number, font: Font) {
  const fontChar = font.characters[c];

  return {
    texture: font.texture,
    x: fontChar.xy[0],
    y: fontChar.xy[1],
    w: fontChar.wh[0],
    h: fontChar.wh[1],
  } as Sprite;
}
