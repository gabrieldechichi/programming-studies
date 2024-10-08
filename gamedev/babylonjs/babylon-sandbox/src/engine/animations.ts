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
export type AnimationRetargetDef = {
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
  retargetDef: AnimationRetargetDef,
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

export async function addTargetedAnimationGroup(
  animationGroups: b.AnimationGroup[],
  idleAnim: TargetAnimationGroupData[],
  transformDict: TransformHierarchyDict,
  retargetDict?: AnimationRetargetDef,
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
