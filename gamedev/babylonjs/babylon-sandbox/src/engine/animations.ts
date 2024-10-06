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

export async function addTargetedAnimationGroup(
  animationGroups: b.AnimationGroup[],
  idleAnim: TargetAnimationGroupData[],
  transformDict: TransformHierarchyDict,
) {
  for (const group of idleAnim) {
    const newGroup = new b.AnimationGroup(group.name);
    for (const anim of group.targetedAnimations) {
      const other = transformDict[anim.targetName];
      newGroup.addTargetedAnimation(anim.animation, other);
    }
    animationGroups.push(newGroup);
  }
}
