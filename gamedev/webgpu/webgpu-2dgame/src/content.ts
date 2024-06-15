import { vec2 } from "gl-matrix";
import { Texture } from "./texture";
import { Font } from "./font/font";

export type Sprite = {
  texture: Texture;
  xy: vec2;
  wh: vec2;
};

export class Content {
  public static playerSprite: Sprite;
  public static spriteSheet: { [name: string]: Sprite } = {};
  public static defaultFont: Font;

  public static async initialize(device: GPUDevice) {
    //load sprite
    {
      const spriteSheetTexture = await Texture.createTextureFromUrl(
        device,
        "assets/Spritesheet/sheet.png",
      );

      const sheetXmlReq = await fetch("assets/Spritesheet/sheet.xml");
      const sheetXmlText = await sheetXmlReq.text();

      const xml = new DOMParser().parseFromString(sheetXmlText, "text/xml");

      // <SubTexture name="beam0.png" x="143" y="377" width="43" height="31"/>
      xml.querySelectorAll("SubTexture").forEach((element) => {
        const name = element.getAttribute("name")!.split(".")[0];
        const x = parseFloat(element.getAttribute("x")!);
        const y = parseFloat(element.getAttribute("y")!);
        const width = parseFloat(element.getAttribute("width")!);
        const height = parseFloat(element.getAttribute("height")!);

        Content.spriteSheet[name] = {
          texture: spriteSheetTexture,
          xy: [x, y],
          wh: [width, height],
        } as Sprite;
      });

      Content.playerSprite = Content.spriteSheet["playerShip1_blue"];
    }

    //load font
    Content.defaultFont = await Font.fromXml(
      device,
      "assets/SpriteFont.png",
      "assets/SpriteFont.xml",
    );
  }
}
