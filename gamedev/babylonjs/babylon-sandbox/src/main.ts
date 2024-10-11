import * as b from "@babylonjs/core";
import * as gui from "@babylonjs/gui";
import "@babylonjs/loaders";
import * as e from "./engine/prelude";
import { Inspector } from "@babylonjs/inspector";
import { FlowGraphAsyncExecutionBlock } from "@babylonjs/core/FlowGraph/flowGraphAsyncExecutionBlock";

const ASPECT_RATIO = 1920 / 1080;

let canvas: HTMLCanvasElement;
let engine: b.Engine;
let scene: b.Scene;
let fpsText: gui.TextBlock;
let fpsContainer: gui.Container;

function update() {
  scene.render();
  fpsText.text = `dt: ${engine.getDeltaTime()}ms\nfps: ${engine.getFps().toFixed(1)}`;

  fpsText.horizontalAlignment = gui.TextBlock.HORIZONTAL_ALIGNMENT_LEFT;
  fpsText.verticalAlignment = gui.TextBlock.VERTICAL_ALIGNMENT_TOP;
  fpsText.leftInPixels =
    -fpsContainer.widthInPixels / 2 + fpsText.widthInPixels / 2;
  fpsText.topInPixels =
    -fpsContainer.heightInPixels / 2 + fpsText.heightInPixels / 2;

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

  const animName = "Idle.glb";
  const xBotAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/models/",
    "XBot.glb",
  );

  const xBotHumanoidDef = await e.loadJson<e.HumanoidSkeletonDef>(
    "/animations/XBot.ht.json",
  );

  // const runAnim = await e.loadHumanoidAnimationData(
  //   "/animations/",
  //   animName,
  //   xBotHumanoidDef,
  // );

  // const xbotMesh = xBotAc.instantiateModelsToScene((name) => name);
  // const xBotTransformDict = buildHierarchyDict(xbotMesh.rootNodes);
  // (xbotMesh.rootNodes[0] as b.TransformNode).position.x += 1.5;
  //
  // {
  //   const posGizmos = new b.PositionGizmo(utilLayer);
  //   posGizmos.attachedNode = xBotTransformDict["mixamorig:RightArm"];
  // }
  //
  // {
  //   const posGizmos = new b.PositionGizmo(utilLayer);
  //   posGizmos.attachedNode = xBotTransformDict["mixamorig:LeftArm"];
  // }
  //
  // e.addTargetedAnimationGroup(
  //   xbotMesh.animationGroups,
  //   runAnim,
  //   xBotTransformDict,
  //   xBotHumanoidDef,
  // );
  //
  // xbotMesh.animationGroups[0].stop();
  // xbotMesh.animationGroups[0].loopAnimation = true;
  // xbotMesh.animationGroups[0].play();
  // xbotMesh.animationGroups[0].loopAnimation = true;

  const cartoonAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/models/",
    "Character_Base.glb",
  );

  cartoonAc.materials[0].dispose();
  const pbrMat = new b.StandardMaterial("Main");
  pbrMat.specularColor = b.Color3.Black();
  const mainTex = new b.Texture("/textures/Colors_Tex.png");
  mainTex.vScale = -1;
  pbrMat.diffuseTexture = mainTex;

  {
    // const cartoonSimpleRetarget = cartoonAc.instantiateModelsToScene(
    //   (name) => name,
    // );
    // (cartoonSimpleRetarget.rootNodes[0] as b.TransformNode).position.x -= 1.5;
    // const cartoonMesh = cartoonSimpleRetarget.rootNodes[0] as b.Mesh;
    // cartoonMesh.getChildMeshes()[0].material = pbrMat;
    // // cartoonMesh.scaling = new b.Vector3(0.5, 0.5, 0.5);
    //
    // const cartoonTransformDict = buildHierarchyDict(
    //   cartoonSimpleRetarget.rootNodes,
    // );
    // const cartomMixamoRetarget = await e.loadJson<e.HumanoidSkeletonDef>(
    //   "/animations/Body_1.ht.json",
    // );
    //
    // e.addTargetedAnimationGroup(
    //   cartoonSimpleRetarget.animationGroups,
    //   runAnim,
    //   cartoonTransformDict,
    //   cartomMixamoRetarget,
    // );
    //
    // cartoonSimpleRetarget.animationGroups[0].stop();
    // cartoonSimpleRetarget.animationGroups[0].loopAnimation = true;
    // cartoonSimpleRetarget.animationGroups[0].play();
    // cartoonSimpleRetarget.animationGroups[0].loopAnimation = true;
  }

  const cartomMixamoRetarget = await e.loadJson<e.HumanoidSkeletonDef>(
    "/animations/Character_Base.ht.json",
  );

  const idleAnimAc = await b.SceneLoader.LoadAssetContainerAsync(
    "/animations/",
    animName,
  );
  const count = 100;
  const maxPerRow = 10;
  const rows = count / maxPerRow;
  for (let x = -maxPerRow / 2; x <= maxPerRow / 2; x++) {
    for (let y = -rows / 2; y <= rows / 2; y++) {
      const cartoonRetargeted = cartoonAc.instantiateModelsToScene(
        (name) => name,
      );
      const cartoonMesh = cartoonRetargeted.rootNodes[0] as b.Mesh;
      cartoonMesh.position.y = 0;
      cartoonMesh.position.x += x;
      cartoonMesh.position.z += y;
      for (const mesh of cartoonMesh.getChildMeshes()) {
        mesh.material = pbrMat;
        // if (mesh.name !== "BODIES") {
        //   for (const submesh of mesh.subMeshes) {
        //     submesh.dispose();
        //   }
        //   mesh.subMeshes = [];
        // }
      }
      // cartoonMesh.scaling = new b.Vector3(0.5, 0.5, 0.5);

      const cartoonTransformDict = buildHierarchyDict(
        cartoonRetargeted.rootNodes,
      );

      const runRetargeted = await e.retargetAnimation(
        idleAnimAc,
        xBotAc.skeletons[0],
        xBotHumanoidDef,
        cartoonAc.skeletons[0]!,
        cartomMixamoRetarget,
        ["LeftUpperArm", "LeftShoulder", "RightUpperArm", "RightShoulder"],
      );

      e.addTargetedAnimationGroup(
        cartoonRetargeted.animationGroups,
        runRetargeted,
        cartoonTransformDict,
      );

      cartoonRetargeted.animationGroups[0].stop();
      cartoonRetargeted.animationGroups[0].loopAnimation = true;
      cartoonRetargeted.animationGroups[0].play();
      cartoonRetargeted.animationGroups[0].loopAnimation = true;
    }
  }

  //load costume
  {
    // const shirtAc = await b.SceneLoader.LoadAssetContainerAsync(
    //   "/models/",
    //   "Shirt_1_Purple.glb",
    // );
    //
    // shirtAc.materials[0].dispose();
    // // const shirtInstance = shirtAc.instantiateModelsToScene((name) => name);
    // // const shirtMesh = shirtInstance.rootNodes[0].getChildMeshes()[0] as b.Mesh;
    // scene.addMesh(shirtAc.meshes[1]);
    // // const shirtMesh = scene.getMeshByUniqueId(shirtAc.meshes[1].uniqueId)!;
    // const shirtMesh = scene.getLastMeshById(shirtAc.meshes[1].id)!;
    // console.log(scene.getMeshesById(shirtAc.meshes[1].id))
    // shirtMesh.skeleton?.dispose();
    // shirtMesh.material = pbrMat;
    // for (const mesh of cartoonMesh.getChildMeshes()) {
    //   if (mesh.name === "GLASSES") {
    //     for (const submesh of mesh.subMeshes) {
    //       if (submesh) {
    //         submesh.dispose();
    //       }
    //     }
    //     mesh.subMeshes = shirtMesh.subMeshes;
    //     shirtMesh.skeleton = mesh.skeleton;
    //     break;
    //   }
    // }
  }

  //ui
  {
    const uiTex = gui.AdvancedDynamicTexture.CreateFullscreenUI("UI");
    fpsContainer = new gui.Container();
    fpsContainer.background = "black";
    fpsContainer.width = 0.5;
    fpsContainer.height = 0.5;
    fpsContainer.horizontalAlignment = gui.Container.HORIZONTAL_ALIGNMENT_LEFT;
    fpsContainer.verticalAlignment = gui.Container.VERTICAL_ALIGNMENT_TOP;
    uiTex.addControl(fpsContainer);

    fpsText = new gui.TextBlock();
    fpsText.text = "FPS";
    fpsText.color = "white";
    fpsText.fontSize = "24";
    fpsText.fontFamily = "Montserrat Black";
    fpsText.horizontalAlignment = gui.TextBlock.HORIZONTAL_ALIGNMENT_LEFT;
    fpsText.verticalAlignment = gui.TextBlock.VERTICAL_ALIGNMENT_TOP;
    fpsText.left = fpsContainer.widthInPixels / 2;
    fpsContainer.addControl(fpsText);
    fpsContainer.widthInPixels = 100;
    fpsContainer.heightInPixels = 100;
  }

  camera.alpha = 1.5;
  camera.beta = 1.3;
  camera.radius = 5.3;

  engine.runRenderLoop(update);
  utilLayer.shouldRender = false;
  // Inspector.Show(scene, {});
}

main();
