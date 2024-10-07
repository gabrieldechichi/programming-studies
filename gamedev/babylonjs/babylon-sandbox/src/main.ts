import * as b from "@babylonjs/core";
import "@babylonjs/loaders";
import * as e from "./engine/prelude";
import { Inspector } from "@babylonjs/inspector";

const ASPECT_RATIO = 1920 / 1080;

let canvas: HTMLCanvasElement;
let engine: b.Engine;
let scene: b.Scene;

function update() {
  scene.render();

  // const camera = scene.cameras[0] as b.ArcRotateCamera
  // camera.alpha = -1.3;
  // camera.beta = 1.3;
  // camera.radius = 10.3;
}

function buildHierarchyDict(rootNodes: b.Node[]): e.TransformHierarchyDict {
  const dict: e.TransformHierarchyDict = {};

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
    e.enforceAspectRatio(canvas, ASPECT_RATIO);
    engine.resize();
  });
  e.enforceAspectRatio(canvas, ASPECT_RATIO);

  engine = new b.Engine(canvas);
  scene = new b.Scene(engine);

  scene.createDefaultLight();
  // const light = new b.DirectionalLight("dir light", new b.Vector3(-2, -3, 0));
  // light.intensity = 1;

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

  // const lightGizmos = new b.LightGizmo(utilLayer);
  // lightGizmos.light = light;
  //
  const xBotAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/models/",
    "XBot.glb",
  );
  const xBotTransformDict = buildHierarchyDict(xBotAc.rootNodes);

  const runAnim = await e.loadTargetedAnimationData(
    "/animations/",
    "Fast Run.glb",
  );

  const xbotMesh = xBotAc.instantiateModelsToScene((name) => name);

  e.addTargetedAnimationGroup(
    xbotMesh.animationGroups,
    runAnim,
    xBotTransformDict,
  );

  xbotMesh.animationGroups[0].stop();
  xbotMesh.animationGroups[0].loopAnimation = true;
  xbotMesh.animationGroups[0].play();
  xbotMesh.animationGroups[0].loopAnimation = true;

  const cartoonAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/models/",
    "Body_4.glb",
  );

  const transformDict = buildHierarchyDict(cartoonAc.rootNodes);
  console.log(transformDict);
  cartoonAc.materials[0].dispose();
  const pbrMat = new b.StandardMaterial("Main");
  pbrMat.specularColor = b.Color3.Black();
  const mainTex = new b.Texture("/textures/Colors_Tex.png");
  mainTex.vScale = -1;
  pbrMat.diffuseTexture = mainTex;
  // pbrMat.albedoTexture = mainTex;
  // pbrMat.albedoColor = b.Color3.White();
  cartoonAc.materials = [pbrMat];

  // (xbotAc.materials[0] as b.PBRMaterial).albedoColor = b.Color3.Green();
  // const idleAnim = await e.loadTargetedAnimationData(
  //   "/animations/",
  //   "Idle.glb",
  // );
  // const runAnim = await e.loadTargetedAnimationData(
  //   "/animations/",
  //   "Fast Run.glb",
  // );

  const cartoon = cartoonAc.instantiateModelsToScene((name) => name);
  cartoon.rootNodes[0].getChildMeshes()[0].material = pbrMat;
  console.log();
  const mesh = cartoon.rootNodes[0] as b.Mesh;

  console.log(mesh);

  camera.alpha = -1.3;
  camera.beta = 1.3;
  camera.radius = 10.3;

  engine.runRenderLoop(update);
  // Inspector.Show(scene, {});
}

main();
