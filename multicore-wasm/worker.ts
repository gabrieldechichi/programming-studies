// Worker thread - receives WASM module and executes functions

// State set during 'load' phase
let wasmInstance: WebAssembly.Instance;
let memory: WebAssembly.Memory;

// Helper to read a string from WASM memory
function readString(ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    const copy = new Uint8Array(bytes);
    return new TextDecoder().decode(copy);
}

self.onmessage = async (e) => {
    const { cmd } = e.data;

    if (cmd === "load") {
        // Phase 1: Receive module and memory, instantiate WASM
        memory = e.data.memory;
        const wasmModule = e.data.wasmModule as WebAssembly.Module;

        const imports = {
            env: {
                memory,
                js_log: (ptr: number, len: number) => {
                    const str = readString(ptr, len);
                    console.log(`[Thread] ${str}`);
                },
                // Workers can't spawn threads (for now)
                js_thread_spawn: () => {
                    console.error("Cannot spawn threads from worker");
                    return -1;
                },
                js_thread_join: () => {
                    console.error("Cannot join threads from worker");
                },
            },
        };

        try {
            // Note: WebAssembly.instantiate with a Module returns Instance directly
            wasmInstance = await WebAssembly.instantiate(wasmModule, imports);
            self.postMessage({ cmd: "loaded" });
        } catch (err) {
            console.error("[Worker] Failed to instantiate WASM:", err);
        }
    } else if (cmd === "run") {
        // Phase 2: Execute the function
        const { funcPtr, argPtr, threadId, doneOffset } = e.data;

        try {
            const table = wasmInstance.exports.__indirect_function_table as WebAssembly.Table;
            const fn = table.get(funcPtr) as (arg: number) => void;

            if (!fn) {
                throw new Error(`Function at index ${funcPtr} is null`);
            }

            fn(argPtr);

            // Signal completion
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, doneOffset / 4, 1);
            Atomics.notify(flags, doneOffset / 4);
        } catch (err) {
            console.error(`[Thread ${threadId}] Error:`, err);
            // Signal completion with error
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, doneOffset / 4, -1);
            Atomics.notify(flags, doneOffset / 4);
        }
    }
};
