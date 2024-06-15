import { vec2 } from "gl-matrix";
import { Texture } from "../texture";

export type FontChar = {
  id: number;
  xy: vec2;
  wh: vec2;
  offset: vec2;
  advance: number;
};

export class Font {
  texture!: Texture;
  lineHeight!: number;
  size!: number;
  characters: { [id: number]: FontChar } = {};

  static async fromXml(
    device: GPUDevice,
    texturePath: string,
    xmlPath: string,
  ): Promise<Font> {
    const font = new Font();
    font.texture = await Texture.createTextureFromUrl(
      device,
      texturePath,
      "linear",
    );

    const xmlTextReq = await fetch(xmlPath);
    const xmlText = await xmlTextReq.text();

    const xml = new DOMParser().parseFromString(xmlText, "text/xml");

    font.lineHeight = parseInt(
      xml.querySelector("common")?.getAttribute("lineHeight")!,
    );
    font.size = parseInt(
      xml.querySelector("info")?.getAttribute("size")!,
    );

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
}
