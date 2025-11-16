import { WasmMemoryInterface } from "./wasm";
import { createFileSystemFunctions } from "./os_file";
import {
  createRendererOSFunctions,
  resizeCanvasToDisplaySize,
  initWebGL2,
} from "./renderer";

// Extend Window interface to include wasmExports
declare global {
  interface Window {
    wasmExports: WasmExports;
  }
}

// Type for WASM exports
interface WasmExports extends WebAssembly.Exports {
  update_and_render: (heapBase: number) => void;
  os_get_heap_base: () => number;
  entrypoint: (heapBase: number, size: bigint) => void;
}

// Type for WASM runtime state
interface WasmRuntime {
  exports: WasmExports | null;
  heapBase: number;
  previousFrameTime: number;
}

resizeCanvasToDisplaySize();
// Handle window resize
window.addEventListener("resize", resizeCanvasToDisplaySize);

const memory = new WebAssembly.Memory({ initial: 16384, maximum: 16384 });
const memInterface = new WasmMemoryInterface(memory);

const canvas = document.getElementById("canvas") as HTMLCanvasElement;

const importObject: WebAssembly.Imports = {
  env: {
    memory: memory,
    ...createFileSystemFunctions(memInterface),
    ...createRendererOSFunctions(memInterface),
    _os_cos: (x: number): number => {
      return Math.cos(x);
    },
    _os_acos: (x: number): number => {
      return Math.acos(x);
    },
    _os_pow: (x: number, y: number): number => {
      return Math.pow(x, y);
    },
    _os_roundf: (x: number): number => {
      return Math.round(x);
    },
    _os_log: (ptr: number, len: number): void => {
      const message = memInterface.loadString(ptr, len);
      console.log(message);
    },
    _os_canvas_width: (): number => {
      // Return CSS width (logical pixels), not physical pixels
      return canvas.clientWidth;
    },
    _os_canvas_height: (): number => {
      // Return CSS height (logical pixels), not physical pixels
      return canvas.clientHeight;
    },
    _os_get_dpr: (): number => {
      return window.devicePixelRatio || 1.0;
    },
  },
};

// WASM runtime state
const wasmRuntime: WasmRuntime = {
  exports: null,
  heapBase: 0,
  previousFrameTime: 0,
};

function renderLoop(currentTime: number): void {
  // Calculate delta time in seconds
  wasmRuntime.previousFrameTime = currentTime;

  // Ensure canvas is at proper resolution
  resizeCanvasToDisplaySize();

  // Call WASM update_and_render
  if (wasmRuntime.exports) {
    wasmRuntime.exports.update_and_render(wasmRuntime.heapBase);
  }

  // Request next frame
  requestAnimationFrame(renderLoop);
}

async function loadWasm(): Promise<void> {
  const response = await fetch("app.wasm");
  const wasmBytes = await response.arrayBuffer();
  const wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);

  wasmRuntime.exports = wasmModule.instance.exports as WasmExports;
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
