import * as b from "@babylonjs/core";

const ASPECT_RATIO = 1920 / 1080;

let canvas: HTMLCanvasElement;
let engine: b.Engine;
let scene: b.Scene;

function enforceAspectRatio() {
  const width = window.innerWidth;
  const height = width / ASPECT_RATIO;
  canvas.width = width;
  canvas.height = height;
}

function update() {
  scene.render();
}

async function main() {
  canvas = document.getElementById("canvas") as HTMLCanvasElement;
  window.addEventListener("resize", (_) => {
    enforceAspectRatio();
    engine.resize();
  });
  enforceAspectRatio();

  engine = new b.Engine(canvas);
  scene = new b.Scene(engine);

  scene.createDefaultCameraOrLight(true, false, true);
  const camera = scene.cameras[0];
  camera.position = new b.Vector3(0, 1, -5);
  b.MeshBuilder.CreateBox("box");

  engine.runRenderLoop(update);
}

main();
