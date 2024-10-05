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

  // const light = new b.PointLight("point light", new b.Vector3(0, 2, 0));
  const light = new b.DirectionalLight("dir light", new b.Vector3(-2, -3, 0));
  light.intensity = 1;

  const camera = new b.UniversalCamera("camera", new b.Vector3(0, 2.5, -3));
  camera.setTarget(new b.Vector3(0, 1, 0));
  camera.attachControl(true);
  camera.inputs.addMouseWheel();

  const utilLayer = new b.UtilityLayerRenderer(scene);

  const sphere = b.MeshBuilder.CreateSphere(
    "sphere",
    { segments: 50, diameter: 1 },
    scene,
  );
  sphere.position.y += 1;

  const ground = b.MeshBuilder.CreateGround("ground", {
    width: 10,
    height: 10,
    subdivisions: 30,
  });
  const groundMat = new b.StandardMaterial("ground mat");
  groundMat.diffuseColor = b.Color3.White();
  // groundMat.ambientColor = b.Color3.White();
  // groundMat.emissiveColor = new b.Color3(0.2);
  ground.material = groundMat;

  const lightGizmos = new b.LightGizmo(utilLayer);
  lightGizmos.light = light;

  const shadowGeneraor = new b.ShadowGenerator(1024, light);
  shadowGeneraor.addShadowCaster(sphere);
  shadowGeneraor.useBlurExponentialShadowMap = true;
  shadowGeneraor.useKernelBlur = true;
  shadowGeneraor.blurKernel = 128;
  ground.receiveShadows = true;

  scene.fogMode = b.Scene.FOGMODE_LINEAR;
  scene.fogStart = -10;
  scene.fogEnd = 40;

  scene.onPointerDown = () => {
    const hit = scene.pick(scene.pointerX, scene.pointerY);
    if (hit.pickedMesh && hit.pickedMesh.name === sphere.name) {
      const mat = (hit.pickedMesh.material ||
        new b.StandardMaterial("sphere")) as b.StandardMaterial;
      if (mat.diffuseColor.b === 0) {
        mat.diffuseColor = b.Color3.White();
      } else {
        mat.diffuseColor = b.Color3.Red();
      }
      hit.pickedMesh.material = mat;
    }
  };

  engine.runRenderLoop(update);
}

main();
