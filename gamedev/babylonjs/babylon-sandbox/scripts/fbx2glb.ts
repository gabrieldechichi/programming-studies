import convert from "fbx2gltf";

function convertFbxToGltf(inputPath: string, outputPath: string) {
  return convert(inputPath, outputPath, ["--khr-materials-unlit"]).then(
    (destPath) => {
      console.log(`Conversion successful! GLB file located at: ${destPath}`);
    },
    (error) => {
      console.error("Conversion failed:", error);
    },
  );
}

// Parse command-line arguments
const args = process.argv.slice(2); // Remove the first two elements (node and script path)
if (args.length !== 2) {
  console.error("Usage: node script.js <inputPath> <outputPath>");
  process.exit(1);
}

const [inputPath, outputPath] = args;

// Execute conversion
convertFbxToGltf(inputPath, outputPath);
