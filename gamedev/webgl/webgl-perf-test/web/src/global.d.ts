interface Odin {
  runWasm: (
    wasmPath: string,
    consoleElement?: HTMLElement,
  ) => Promise<void>;
}

interface Window {
  odin: Odin;
}
