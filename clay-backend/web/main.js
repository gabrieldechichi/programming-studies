const canvas = document.getElementById('canvas');
const gl = canvas.getContext('webgl2');

const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });

const importObject = {
    env: {
        memory: memory,
        js_log: (value) => {
            console.log('[WASM]:', value);
        },
    }
};

async function loadWasm() {
    const response = await fetch('app.wasm');
    const wasmBytes = await response.arrayBuffer();
    const wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);

    const result = wasmModule.instance.exports.entrypoint();
    console.log('entrypoint() returned:', result);

    window.wasmExports = wasmModule.instance.exports;
}

loadWasm();
