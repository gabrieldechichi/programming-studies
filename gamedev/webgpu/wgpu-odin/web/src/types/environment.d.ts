import { WasmMemoryInterface, WebGLInterface, runWasm } from "./runtime";

export interface OdinType {
  WasmMemoryInterface: WasmMemoryInterface & { new (): WasmMemoryInterface };
  WebGLInterface: WebGLInterface;
  runWasm: typeof runWasm;
}

declare global {
  interface Window {
    odin: OdinType;
  }
}
