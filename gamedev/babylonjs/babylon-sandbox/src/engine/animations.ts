import * as b from "@babylonjs/core";
import "@babylonjs/loaders";

export type TargetedAnimationData = {
  targetName: string;
  animation: b.Animation;
};

export type TargetAnimationGroupData = {
  name: string;
  targetedAnimations: TargetedAnimationData[];
};

export type TransformHierarchyDict = Record<string, b.Node>;
export type HumanoidSkeletonDef = {
  boneToHuman: Record<string, string>;
  humanToBone: Record<string, string>;
};

export async function loadTargetedAnimationData(path: string, glb: string) {
  const asset = await b.SceneLoader.LoadAssetContainerAsync(path, glb);
  const groups = asset.animationGroups.map(
    (g) =>
      ({
        name: g.name,
        targetedAnimations: g.targetedAnimations.map((a) => ({
          targetName: a.target.name,
          animation: a.animation,
        })),
      }) as TargetAnimationGroupData,
  );
  asset.dispose();
  return groups;
}

export async function loadHumanoidAnimationData(
  path: string,
  glb: string,
  retargetDef: HumanoidSkeletonDef,
) {
  const asset = await b.SceneLoader.LoadAssetContainerAsync(path, glb);
  const groups = asset.animationGroups.map((g) => {
    return {
      name: g.name,
      targetedAnimations: g.targetedAnimations.map((a) => {
        const targetName = retargetDef.boneToHuman[a.target.name];
        if (!targetName) {
          throw new Error(`No humanoid definition for bone ${a.target.name}`);
        }
        return {
          targetName,
          animation: a.animation,
        };
      }),
    } as TargetAnimationGroupData;
  });
  asset.dispose();
  return groups;
}

export async function loadAnimationWithRetarget(
  path: string,
  glb: string,
  sourceSkeleton: b.Skeleton,
  sourceHumanoidDef: HumanoidSkeletonDef,
  targetSkeleton: b.Skeleton,
  targetHumanoidDef: HumanoidSkeletonDef,
) {
  const asset = await b.SceneLoader.LoadAssetContainerAsync(path, glb);
  const groups = asset.animationGroups.map((g) => {
    return {
      name: g.name,
      targetedAnimations: g.targetedAnimations
        .map((a) => {
          const sourceBone = sourceSkeleton.bones.find(
            (b) => b.name === a.target.name,
          );
          if (!sourceBone) {
            throw new Error(
              `Couldn't find source bone with name ${a.target.name}`,
            );
          }
          const humanBoneName = sourceHumanoidDef.boneToHuman[a.target.name];
          if (!humanBoneName) {
            throw new Error(`No humanoid definition for bone ${a.target.name}`);
          }
          const targetBoneName = targetHumanoidDef.humanToBone[humanBoneName];

          if (!targetBoneName) {
            return null;
          }
          const targetBone = targetSkeleton.bones.find(
            (b) => b.name === targetBoneName,
          );

          if (!targetBone) {
            throw new Error(
              `Couldn't find bone with name ${targetBoneName} on target skeleton`,
            );
          }

          retargetAnimationForBone(a.animation, sourceBone, targetBone);

          return {
            targetName: targetBoneName,
            animation: a.animation,
          };
        })
        .filter((a) => a),
    } as TargetAnimationGroupData;
  });
  asset.dispose();
  return groups;
}

export async function addTargetedAnimationGroup(
  animationGroups: b.AnimationGroup[],
  idleAnim: TargetAnimationGroupData[],
  transformDict: TransformHierarchyDict,
  retargetDict?: HumanoidSkeletonDef,
) {
  for (const group of idleAnim) {
    const newGroup = new b.AnimationGroup(group.name);
    for (const anim of group.targetedAnimations) {
      let targetName = anim.targetName;
      if (retargetDict) {
        targetName = retargetDict.humanToBone[targetName];
      }
      if (targetName && targetName !== "") {
        const other = transformDict[targetName];
        newGroup.addTargetedAnimation(anim.animation, other);
      }
    }
    animationGroups.push(newGroup);
  }
}

function retargetAnimationForBone(
  animation: b.Animation,
  srcBone: b.Bone,
  dstBone: b.Bone,
) {
  const tmpMatrix = b.TmpVectors.Matrix[0];
  const tmpMatrix2 = b.TmpVectors.Matrix[1];
  const tmpMatrix3 = b.TmpVectors.Matrix[2];

  const srcInvLocalBindMatrix = srcBone.getBindMatrix().invertToRef(tmpMatrix);
  const dstLocalBindMatrix = dstBone.getBindMatrix();

  //dstBone.getInvertedAbsoluteTransform().copyFrom(b.Matrix.IdentityReadOnly);

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

        const mat = b.Matrix.TranslationToRef(
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

        const mat = b.Matrix.FromQuaternionToRef(key.value, tmpMatrix3);
        const matr = dstLocalBindMatrix
          .multiplyToRef(srcInvLocalBindMatrix, tmpMatrix2)
          .multiplyToRef(mat, tmpMatrix2);
        //const matr = srcInvLocalBindMatrix.multiply(mat);
        matr.decompose(undefined, key.value, undefined);
        break;
      }
      case "scaling": {
        key.value = key.value.clone();

        const mat = b.Matrix.ScalingToRef(
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
