// Create memory (16MB initial, will be shared later for threads)
const memory = new WebAssembly.Memory({ initial: 256 });

// Helper to read a string from WASM memory
function readString(ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

// Import object - functions provided to WASM
const imports = {
    env: {
        memory,
        js_log: (ptr: number, len: number) => {
            const str = readString(ptr, len);
            console.log(str);
        },
    },
};

// Load and run (works in both browser and bun)
const wasmUrl = new URL("./wasm.wasm", import.meta.url);
const wasmBytes = await (await fetch(wasmUrl)).arrayBuffer();
const { instance } = await WebAssembly.instantiate(wasmBytes, imports);

const main = instance.exports.main as () => number;
const result = main();
console.log(`main() returned: ${result}`);
