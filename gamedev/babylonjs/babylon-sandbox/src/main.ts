import * as b from "@babylonjs/core";
import "@babylonjs/loaders";
import { Inspector } from "@babylonjs/inspector";

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
  const cam = scene.cameras[0] as b.ArcRotateCamera;
  cam.alpha = 1.3;
  cam.beta = 1.3;
}

function buildHierarchyDict(rootNodes: b.Node[]) {
  const dict: Record<string, b.Node> = {};

  for (const root of rootNodes) {
    buildHierarchyDictRecursive(root);
  }

  function buildHierarchyDictRecursive(node: b.Node) {
    dict[node.name] = node;
    for (const child of node.getChildren()) {
      buildHierarchyDictRecursive(child);
    }
  }

  return dict;
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

  const light = new b.DirectionalLight("dir light", new b.Vector3(-2, -3, 0));
  light.intensity = 1;

  const camera = new b.ArcRotateCamera(
    "camera",
    1.3,
    1.3,
    10,
    new b.Vector3(0, 1, 0),
    scene,
  );
  camera.attachControl(true);
  // //camera.inputs.addMouseWheel();
  // //camera.setTarget(b.Vector3.Zero());

  camera.setPosition(new b.Vector3(0, 0, -2));

  const utilLayer = new b.UtilityLayerRenderer(scene);

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

  const xbotAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/models/",
    "XBot.glb",
  );

  const idleAnimAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/animations/",
    "Idle.glb",
  );

  const xbot = xbotAc.instantiateModelsToScene((name) => name);
  const transformDict = buildHierarchyDict(xbot.rootNodes);
  console.log(transformDict);

  const idle = idleAnimAc.instantiateModelsToScene((name) => name);
  for (const group of idle.animationGroups) {
    for (const anim of group.targetedAnimations) {
      const other = transformDict[anim.target.name];
      anim.target = other
    }
  }
  idle.animationGroups[0].stop()
  idle.animationGroups[0].play()

  engine.runRenderLoop(update);
  Inspector.Show(scene, {});
}

main();
