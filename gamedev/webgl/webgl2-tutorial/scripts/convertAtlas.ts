import * as core from "../src/core.ts";
import * as graphics from "../src/graphics.ts";
import fs from "node:fs";

async function main() {
  const atlasStr = fs.readFileSync("./assets/textures/atlas_bk.json").toString();
  const atlas = JSON.parse(atlasStr) as Record<string, number[]>;
  const atlasSize = { w: 1024, h: 2048 };
  const w = 128;
  const h = 128;

  const spriteAtlas: Record<string, graphics.SpriteRegion> = {};

  for (const key of Object.keys(atlas)) {
    const [u, v] = atlas[key];
    const spriteRegion = {
      x: u * atlasSize.w,
      y: v * atlasSize.h,
      w,
      h,
    } as graphics.SpriteRegion;
    spriteAtlas[key] = spriteRegion;
  }

  const spriteAtlasStr = JSON.stringify(spriteAtlas);
  fs.writeFileSync("./assets/textures/atlas.json", spriteAtlasStr);

  console.log(spriteAtlas);
}

main().catch((err) => console.error(err));
