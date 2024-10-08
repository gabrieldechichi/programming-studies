var createScene = async function () {
  // This creates a basic Babylon Scene object (non-mesh)
  var scene = new BABYLON.Scene(engine);
  scene.useRightHandedSystem = true;

  // This creates and positions a free camera (non-mesh)
  var camera = new BABYLON.FreeCamera(
    "camera1",
    new BABYLON.Vector3(0, 5, -15),
    scene,
  );

  // This targets the camera to scene origin
  camera.setTarget(BABYLON.Vector3.Zero());

  // This attaches the camera to the canvas
  camera.attachControl(canvas, true);

  // This creates a light, aiming 0,1,0 - to the sky (non-mesh)
  var light = new BABYLON.HemisphericLight(
    "light",
    new BABYLON.Vector3(0, 1, 0.5),
    scene,
  );

  // Default intensity is 1. Let's dim the light a small amount
  light.intensity = 0.7;

  await BABYLON.SceneLoader.AppendAsync(
    "https://preview.punkoffice.com/worm/",
    "worm_base.glb",
    scene,
  );

  scene.getMeshByName("__root__").position.x -= 6;

  await BABYLON.SceneLoader.AppendAsync(
    "https://preview.punkoffice.com/worm/",
    "worm_target.glb",
    scene,
  );

  scene.createDefaultCamera(true, true, true);

  const srcSkel = scene.skeletons[0];
  const dstSkel = scene.skeletons[1];

  const sourceToDestBoneRemapping = buildBoneRemappingFromSrcBoneNames(
    srcSkel,
    dstSkel,
  );

  let newAnimGroup = retargetAnimationGroup(
    scene.animationGroups[0],
    srcSkel,
    sourceToDestBoneRemapping,
    false,
  );

  if (typeof newAnimGroup === "string") {
    console.error(newAnimGroup);
    console.error("Could not retarget the animation group!");
    newAnimGroup = null;
  }

  setTimeout(() => {
    scene.animationGroups[0].pause();
    scene.animationGroups[0].goToFrame(50);

    newAnimGroup?.play();
    newAnimGroup?.goToFrame(50);
    newAnimGroup?.pause();
  }, 2000);

  return scene;
};

function findBoneByName(skeleton, name) {
  for (const bone of skeleton.bones) {
    if (bone.name === name) {
      return bone;
    }
  }
  return null;
}

function buildBoneRemappingFromSrcBoneNames(srcSkel, dstSkel) {
  const remapping = new Map();
  const srcBones = srcSkel.bones;

  for (let i = 0; i < srcBones.length; ++i) {
    const srcBone = srcBones[i];
    const dstBone = findBoneByName(dstSkel, srcBone.name);

    remapping.set(srcBone, dstBone);
  }

  return remapping;
}

function buildTransformNodeToBoneMap(skeleton) {
  const map = new Map();
  for (const bone of skeleton.bones) {
    const transformNode = bone.getTransformNode();
    if (transformNode) {
      map.set(transformNode, bone);
    }
  }
  return map;
}

function retargetAnimationGroup(
  animationGroup,
  srcSkel,
  sourceToDestBoneRemapping,
  errorIfRemappedBonedNotFound,
) {
  const newAnimGroup = animationGroup.clone(
    "retarget of " + animationGroup.name,
    undefined,
    true,
  );

  const srcTransformNodeToBone = buildTransformNodeToBoneMap(srcSkel);

  const targetedAnimations = newAnimGroup.targetedAnimations;

  for (
    let targetedAnimationIndex = 0;
    targetedAnimationIndex < targetedAnimations.length;
    ++targetedAnimationIndex
  ) {
    const targetedAnimation = targetedAnimations[targetedAnimationIndex];
    const target = targetedAnimation.target;
    const animation = targetedAnimation.animation;

    const srcBone = srcTransformNodeToBone.get(target);
    if (!srcBone) {
      throw (
        `Can't find the bone corresponding to the transform node with id ${target.id} in the source skeleton !` +
        `Make sure you retarget an animation group for a skeleton.`
      );
    }

    const dstBone = sourceToDestBoneRemapping.get(srcBone);
    if (!dstBone) {
      if (errorIfRemappedBonedNotFound) {
        return `Can't find the bone in the destination skeleton corresponding to the source bone "${srcBone.name}" !`;
      }
      targetedAnimations.splice(targetedAnimationIndex, 1);
      targetedAnimationIndex--;
      continue;
    }

    // Update the target to be the bone in the destination skeleton
    targetedAnimation.target = dstBone.getTransformNode();

    // Retarget the animation to the destination bone
    retargetAnimationForBone(animation, srcBone, dstBone);
  }

  return newAnimGroup;
}

function retargetAnimationForBone(animation, srcBone, dstBone) {
  const tmpMatrix = BABYLON.TmpVectors.Matrix[0];
  const tmpMatrix2 = BABYLON.TmpVectors.Matrix[1];
  const tmpMatrix3 = BABYLON.TmpVectors.Matrix[2];

  const srcInvLocalBindMatrix = srcBone.getBaseMatrix().invertToRef(tmpMatrix);
  const dstLocalBindMatrix = dstBone.getBaseMatrix();

  //dstBone.getInvertedAbsoluteTransform().copyFrom(BABYLON.Matrix.IdentityReadOnly);

  const keys = animation.getKeys();
  for (let keyIndex = 0; keyIndex < keys.length; ++keyIndex) {
    const keySrc = keys[keyIndex];

    const key = {
      frame: keySrc.frame,
      value: keySrc.value,
      inTangent: keySrc.inTangent,
      outTangent: keySrc.outTangent,
      interpolation: keySrc.interpolation,
      lockedTangent: keySrc.lockedTangent,
    };
    keys[keyIndex] = key;

    switch (animation.targetProperty) {
      case "position": {
        key.value = key.value.clone();

        const mat = BABYLON.Matrix.TranslationToRef(
          key.value.x,
          key.value.y,
          key.value.z,
          tmpMatrix3,
        );
        const matr = dstLocalBindMatrix
          .multiplyToRef(srcInvLocalBindMatrix, tmpMatrix2)
          .multiplyToRef(mat, tmpMatrix2);
        //const matr = srcInvLocalBindMatrix.multiply(mat);
        matr.decompose(undefined, undefined, key.value);
        break;
      }
      case "rotationQuaternion": {
        key.value = key.value.clone();

        const mat = BABYLON.Matrix.FromQuaternionToRef(key.value, tmpMatrix3);
        const matr = dstLocalBindMatrix
          .multiplyToRef(srcInvLocalBindMatrix, tmpMatrix2)
          .multiplyToRef(mat, tmpMatrix2);
        //const matr = srcInvLocalBindMatrix.multiply(mat);
        matr.decompose(undefined, key.value, undefined);
        break;
      }
      case "scaling": {
        key.value = key.value.clone();

        const mat = BABYLON.Matrix.ScalingToRef(
          key.value.x,
          key.value.y,
          key.value.z,
          tmpMatrix3,
        );
        const matr = dstLocalBindMatrix
          .multiplyToRef(srcInvLocalBindMatrix, tmpMatrix2)
          .multiplyToRef(mat, tmpMatrix2);
        //const matr = srcInvLocalBindMatrix.multiply(mat);
        matr.decompose(key.value, undefined, undefined);
        break;
      }
    }
  }
}
