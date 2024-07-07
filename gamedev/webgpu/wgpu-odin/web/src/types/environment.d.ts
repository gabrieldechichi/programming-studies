import { WasmMemoryInterface, WebGLInterface } from "./runtime";

export interface OdinType {
  WasmMemoryInterface: WasmMemoryInterface & { new (): WasmMemoryInterface };
  WebGLInterface: WebGLInterface;
}

declare global {
  interface Window {
    odin: OdinType;
  }
}
