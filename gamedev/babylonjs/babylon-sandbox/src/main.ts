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
  // scene.createDefaultLight();
  // scene.ambientColor = new b.Color3(0, 0.6, 0.6);

  const light = new b.PointLight("point light", new b.Vector3(0, 2, 0));
  light.range = 100;
  light.intensity = 0.5;

  const camera = new b.UniversalCamera("camera", new b.Vector3(0, 1, -5));
  camera.setTarget(new b.Vector3(0, 0, 0));
  camera.attachControl(true);
  camera.inputs.addMouseWheel();
  const box = b.MeshBuilder.CreateBox("box");
  box.position.y += box.getBoundingInfo().boundingBox.extendSizeWorld.y;

  const boxMat = new b.StandardMaterial("box mat");
  boxMat.diffuseColor = b.Color3.White();
  boxMat.ambientColor = b.Color3.White();
  box.material = boxMat;

  const utilLayer = new b.UtilityLayerRenderer(scene);
  const positionGizmos = new b.PositionGizmo(utilLayer);
  positionGizmos.attachedMesh = box;

  const ground = b.MeshBuilder.CreateGround("ground", {
    width: 10,
    height: 10,
    subdivisions: 30,
  });

  const groundMat = new b.StandardMaterial("ground mat");
  groundMat.diffuseColor = b.Color3.White();
  groundMat.ambientColor = b.Color3.White();
  groundMat.emissiveColor = new b.Color3(0.2);
  ground.material = groundMat;

  const lightGizmos = new b.LightGizmo(utilLayer);
  lightGizmos.light = light;

  engine.runRenderLoop(update);
}

main();
