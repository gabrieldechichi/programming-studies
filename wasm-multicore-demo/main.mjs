// main.ts
async function init() {
  const outputEl = document.getElementById("output");
  const originalLog = console.log;
  console.log = (...args) => {
    originalLog.apply(console, args);
    outputEl.textContent += args.join(" ") + `
`;
  };
  try {
    const Module = await createModule();
    console.log("Module loaded successfully!");
    const helloBtn = document.getElementById("hello-btn");
    helloBtn.addEventListener("click", () => {
      Module.ccall("hello", null, [], []);
    });
  } catch (error) {
    console.error("Failed to load WASM module:", error);
  }
}
init();
