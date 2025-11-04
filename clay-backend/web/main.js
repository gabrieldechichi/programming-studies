import { WasmMemoryInterface } from "./wasm.js";
import { createFileSystemFunctions } from "./os_file.js";
import {
  createRendererOSFunctions,
  resizeCanvasToDisplaySize,
  initWebGL2,
} from "./renderer.js";

resizeCanvasToDisplaySize();
// Handle window resize
window.addEventListener("resize", resizeCanvasToDisplaySize);

const memory = new WebAssembly.Memory({ initial: 16384, maximum: 16384 });
const memInterface = new WasmMemoryInterface(memory);

const importObject = {
  env: {
    memory: memory,
    ...createFileSystemFunctions(memInterface),
    ...createRendererOSFunctions(memInterface),
    _os_cos: (x) => {
      return Math.cos(x);
    },
    _os_acos: (x) => {
      return Math.acos(x);
    },
    _os_pow: (x, y) => {
      return Math.pow(x, y);
    },
    _os_roundf: (x) => {
      return Math.round(x);
    },
    _os_log: (ptr, len) => {
      const message = memInterface.loadString(ptr, len);
      console.log(message);
    },
    _os_canvas_width: () => {
      // Return CSS width (logical pixels), not physical pixels
      return canvas.clientWidth;
    },
    _os_canvas_height: () => {
      // Return CSS height (logical pixels), not physical pixels
      return canvas.clientHeight;
    },
    _os_get_dpr: () => {
      return window.devicePixelRatio || 1.0;
    },
  },
};

// WASM runtime state
const wasmRuntime = {
  exports: null,
  heapBase: 0,
  previousFrameTime: 0,
};

function renderLoop(currentTime) {
  // Calculate delta time in seconds
  wasmRuntime.previousFrameTime = currentTime;

  // Ensure canvas is at proper resolution
  resizeCanvasToDisplaySize();

  // Call WASM update_and_render
  wasmRuntime.exports.update_and_render(wasmRuntime.heapBase);

  // Request next frame
  requestAnimationFrame(renderLoop);
}

async function loadWasm() {
  const response = await fetch("app.wasm");
  const wasmBytes = await response.arrayBuffer();
  const wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);

  wasmRuntime.exports = wasmModule.instance.exports;
  window.wasmExports = wasmRuntime.exports;

  // Get heap base pointer
  wasmRuntime.heapBase = wasmRuntime.exports.os_get_heap_base();

  // Initialize WebGL2
  initWebGL2();

  // Call entrypoint with memory pointer
  wasmRuntime.exports.entrypoint(
    wasmRuntime.heapBase,
    BigInt(memory.buffer.byteLength - wasmRuntime.heapBase),
  );

  // Start the render loop
  requestAnimationFrame(renderLoop);
}

loadWasm();
