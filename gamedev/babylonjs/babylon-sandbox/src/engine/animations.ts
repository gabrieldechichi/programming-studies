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
  skipRetargetBoneNames?: string[],
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

          if (
            !skipRetargetBoneNames ||
            skipRetargetBoneNames.indexOf(humanBoneName) === -1
          ) {
            retargetAnimationForBone(a.animation, sourceBone, targetBone);
          }

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

export async function loadAnimationWithRetarget2(
  path: string,
  glb: string,
  sourceHierarchy: TransformHierarchyDict,
  sourceHumanoidDef: HumanoidSkeletonDef,
  targetHierarchy: TransformHierarchyDict,
  targetHumanoidDef: HumanoidSkeletonDef,
) {
  const asset = await b.SceneLoader.LoadAssetContainerAsync(path, glb);
  const groups = asset.animationGroups.map((g) => {
    return {
      name: g.name,
      targetedAnimations: g.targetedAnimations
        .map((a) => {
          const sourceBone = sourceHierarchy[a.target.name] as b.TransformNode;
          const humanBoneName = sourceHumanoidDef.boneToHuman[a.target.name];
          const targetBoneName = targetHumanoidDef.humanToBone[humanBoneName];
          if (!targetBoneName) {
            return null;
          }
          const targetBone = targetHierarchy[targetBoneName] as b.TransformNode;

          if (!sourceBone) {
            throw new Error(`Couldn't find source bone ${a.target.name}`);
          }
          if (!targetBone) {
            throw new Error(`Couldn't find target bone ${targetBoneName}`);
          }

          retargetAnimationForBone2(a.animation, sourceBone, targetBone);

          return {
            targetName: targetBoneName,
            animation: a.animation,
          } as TargetedAnimationData;
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
  const bindMatrix = srcBone.getRestMatrix();
  const inverseBindMatrix = bindMatrix.invertToRef(b.TmpVectors.Matrix[0]);
  const targetMatrix = dstBone.getRestMatrix();
  const inverseParentMatrix = computeInverseParentMatrixRecursiveBoneToTarget(
    dstBone,
    b.TmpVectors.Matrix[1],
  );

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
        const mat = mat4Trs(
          key.value as b.Vector3,
          b.Quaternion.FromEulerVector(srcBone.rotation),
          srcBone.scaling,
        );

        let localMatrix = mat4Multiply(
          inverseBindMatrix,
          computeWorldMatrixRecursiveBone(srcBone, mat),
        );

        localMatrix = mat4Multiply(
          targetMatrix,
          mat4Multiply(localMatrix, inverseParentMatrix),
        );

        // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        localMatrix.decompose(undefined, undefined, key.value);

        break;
      }
      case "rotationQuaternion": {
        const quat = key.value.clone() as b.Quaternion;

        const srcRestPoseQuaternion = srcBone.getRotationQuaternion(
          b.Space.LOCAL,
        );
        const dstRestPoseQuaternion = dstBone.getRotationQuaternion(
          b.Space.LOCAL,
        );

        const poseOffsetQuaternion = srcRestPoseQuaternion
          .invert()
          .multiply(dstRestPoseQuaternion);

        quat.multiplyInPlace(poseOffsetQuaternion);
        quat.normalize();
        key.value = quat;

        //stuff
        {
          // const mat = mat4Trs(srcBone.position, quat, srcBone.scaling);
          //
          // let localMatrix = mat4Multiply(
          //   inverseBindMatrix,
          //   computeWorldMatrixRecursiveBone(srcBone, mat),
          // );
          //
          // localMatrix = mat4Multiply(
          //   targetMatrix,
          //   mat4Multiply(localMatrix, inverseParentMatrix),
          // );
          //
          // // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
          // localMatrix.decompose(undefined, key.value, undefined);
        }

        break;
      }
      case "scaling": {
        // key.value = key.value.clone();
        // const mat = b.Matrix.ScalingToRef(
        //   key.value.x,
        //   key.value.y,
        //   key.value.z,
        //   boneMatrix,
        // );
        //
        // // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        // const newMat = mat
        //   .multiplyToRef(bindMatrix, mat)
        //   .multiplyToRef(dstNodeWTL, mat);
        // newMat.decompose(key.value, undefined, undefined);
        break;
      }
      default:
        throw new Error(animation.targetProperty);
    }
  }
}

export function mat4Multiply(a: b.Matrix, b: b.Matrix) {
  return a.multiply(b);
}

function getLocalMatrix(t: b.TransformNode) {
  return mat4Trs(
    t.position,
    b.Quaternion.FromEulerVector(t.rotation),
    t.scaling,
  );
}

function mat4Trs(t: b.Vector3, r: b.Quaternion, s: b.Vector3) {
  let rot = b.Matrix.Identity();

  rot = b.Matrix.FromQuaternionToRef(r, rot);

  const translation = b.Matrix.Translation(t.x, t.y, t.z);

  const scale = b.Matrix.Scaling(s.x, s.y, s.z);
  return mat4Multiply(scale, mat4Multiply(rot, translation));
}

function retargetAnimationForBone2(
  animation: b.Animation,
  srcBone: b.TransformNode,
  dstBone: b.TransformNode,
) {
  const bindMatrix = computeWorldMatrixRecursive(
    srcBone,
    getLocalMatrix(srcBone),
  );
  const inverseBindMatrix = bindMatrix.clone().invert();
  const targetMatrix = computeWorldMatrixRecursive(
    dstBone,
    getLocalMatrix(dstBone),
  );
  const inverseParentMatrix = computeInverseParentMatrixRecursive(dstBone);

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
        const mat = mat4Trs(
          key.value as b.Vector3,
          b.Quaternion.FromEulerVector(srcBone.rotation),
          srcBone.scaling,
        );

        let localMatrix = mat4Multiply(
          inverseBindMatrix,
          computeWorldMatrixRecursive(srcBone, mat),
        );

        localMatrix = mat4Multiply(
          targetMatrix,
          mat4Multiply(localMatrix, inverseParentMatrix),
        );

        // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        localMatrix.decompose(undefined, undefined, key.value);

        break;
      }
      case "rotationQuaternion": {
        key.value = key.value.clone();
        const mat = mat4Trs(
          srcBone.position,
          key.value as b.Quaternion,
          srcBone.scaling,
        );

        let localMatrix = mat4Multiply(
          inverseBindMatrix,
          computeWorldMatrixRecursive(srcBone, mat),
        );
        localMatrix = mat4Multiply(
          targetMatrix,
          mat4Multiply(localMatrix, inverseParentMatrix),
        );

        localMatrix.decompose(undefined, key.value, undefined);
        break;
      }
      case "scaling": {
        key.value = key.value.clone();
        const mat = mat4Trs(
          srcBone.position,
          b.Quaternion.FromEulerVector(srcBone.rotation),
          key.value as b.Vector3,
        );

        let localMatrix = mat4Multiply(
          inverseBindMatrix,
          computeWorldMatrixRecursive(srcBone, mat),
        );

        localMatrix = mat4Multiply(
          targetMatrix,
          mat4Multiply(localMatrix, inverseParentMatrix),
        );

        // Apply the retargeting formula: dstBindPose * srcBindPoseInverse * sourceLocalTransform * srcBindPose * dstBindPoseInverse
        localMatrix.decompose(key.value, undefined, undefined);

        break;
      }
      default:
        throw new Error(animation.targetProperty);
    }
  }
}

function computeWorldMatrixRecursive(
  srcBone: b.TransformNode,
  startingMatrix: b.Matrix,
) {
  let matrix = startingMatrix.clone();
  let parent = srcBone.parent as b.TransformNode;
  while (parent) {
    matrix = mat4Multiply(matrix, getLocalMatrix(parent));
    parent = parent.parent as b.TransformNode;
  }
  return matrix;
}

function computeInverseParentMatrixRecursive(srcBone: b.TransformNode) {
  let matrix = b.Matrix.Identity();
  let parent = srcBone.parent as b.TransformNode;
  if (parent) {
    while (parent) {
      matrix = mat4Multiply(matrix, getLocalMatrix(parent));
      parent = parent.parent as b.TransformNode;
    }
    matrix.invert();
  }
  return matrix;
}

function computeWorldMatrixRecursiveBone(
  srcBone: b.Bone,
  startingMatrix: b.Matrix,
) {
  let matrix = startingMatrix.clone();
  let parent = srcBone.parent;
  while (parent) {
    matrix = mat4Multiply(matrix, parent.getLocalMatrix());
    parent = parent.parent;
  }
  return matrix;
}

function computeInverseParentMatrixRecursiveBoneToTarget(
  srcBone: b.Bone,
  matrix: b.Matrix,
) {
  let parent = srcBone.parent;
  if (parent) {
    while (parent) {
      matrix = mat4Multiply(matrix, parent.getLocalMatrix());
      parent = parent.parent;
    }
    matrix.invert();
  }
  return matrix;
}
