import * as yaml from "js-yaml";
import * as fs from "fs";

export type AnimationRetargetDef = {
  boneToHuman: Record<string, string>;
  humanToBone: Record<string, string>;
};

// Retrieve command-line arguments
const args = process.argv.slice(2);
if (args.length < 2) {
  console.error("Please provide input and output file paths.");
  process.exit(1);
}

const [inputFilePath, outputFilePath] = args;
const optionalPrefix = args[2];

try {
  // Load the YAML file
  const fileContent = fs.readFileSync(inputFilePath, "utf8");

  // Pre-process to remove the initial non-YAML lines
  const processedContent = fileContent.split("\n").slice(3).join("\n");
  const parsedYaml = yaml.load(processedContent) as any;

  // Extract the m_BoneTemplate key-value pairs
  const mBoneTemplate = parsedYaml.HumanTemplate.m_BoneTemplate;

  // Create both mappings: boneToHuman and humanToBone
  const boneToHuman: Record<string, string> = {};
  const humanToBone: Record<string, string> = {};

  for (let [humanKey, boneValue] of Object.entries(mBoneTemplate)) {
    if (optionalPrefix) {
      boneValue = `${optionalPrefix}:${boneValue}`;
    }
    boneToHuman[boneValue as string] = humanKey as string;
    humanToBone[humanKey as string] = boneValue as string;
  }

  // Structured result as per the AnimationRetargetDef type
  const result: AnimationRetargetDef = {
    boneToHuman,
    humanToBone,
  };

  // Convert to JSON
  const jsonResult = JSON.stringify(result, null, 2);

  // Write the output to a file
  fs.writeFileSync(outputFilePath, jsonResult);

  console.log(`Reversed JSON has been written to ${outputFilePath}`);
} catch (e) {
  console.error("Error processing the YAML file:", e);
}
