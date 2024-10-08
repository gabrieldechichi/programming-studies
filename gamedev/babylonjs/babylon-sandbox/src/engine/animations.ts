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
  let boneMatrix = b.TmpVectors.Matrix[0];
  const srcNodeLTW = srcBone.getRestMatrix();
  const dstNodeWTL = dstBone
    .getRestMatrix()
    .invertToRef(b.TmpVectors.Matrix[1]);

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
          boneMatrix,
        );

        // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        const newMat = mat
          .multiplyToRef(srcNodeLTW, mat)
          .multiplyToRef(dstNodeWTL, mat);
        newMat.decompose(undefined, undefined, key.value);

        break;
      }
      case "rotationQuaternion": {
        const quat = key.value.clone() as b.Quaternion;

        // const srcRestPoseQuaternion = srcBone.getRotationQuaternion(
        //   b.Space.WORLD,
        // );
        // const dstRestPoseQuaternion = dstBone.getRotationQuaternion(
        //   b.Space.WORLD,
        // );

        const srcRestPoseQuaternion = b.Quaternion.FromRotationMatrix(
          srcBone.getRestMatrix(),
        );
        const dstRestPoseQuaternion = b.Quaternion.FromRotationMatrix(
          dstBone.getRestMatrix(),
        );

        const poseOffsetQuaternion = srcRestPoseQuaternion
          .conjugate()
          .multiply(dstRestPoseQuaternion);

        quat.multiplyInPlace(poseOffsetQuaternion);
        quat.normalize();

        // const mat = b.Matrix.FromQuaternionToRef(quat, boneMatrix);
        // const newMat = mat
        //   .multiplyToRef(srcNodeLTW, mat)
        //   .multiplyToRef(dstNodeWTL, mat);
        //
        // newMat.decompose(undefined, quat, undefined);

        poseOffsetQuaternion.normalize()
        // key.value = srcRestPoseQuaternion;
        break;
      }
      case "scaling": {
        key.value = key.value.clone();
        const mat = b.Matrix.ScalingToRef(
          key.value.x,
          key.value.y,
          key.value.z,
          boneMatrix,
        );

        // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        const newMat = mat
          .multiplyToRef(srcNodeLTW, mat)
          .multiplyToRef(dstNodeWTL, mat);

        newMat.decompose(key.value, undefined, undefined);
        break;
      }
    }
  }
}
