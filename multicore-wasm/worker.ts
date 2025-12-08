// Worker thread - receives WASM module and executes functions

// State set during 'load' phase
let wasmInstance: WebAssembly.Instance;
let memory: WebAssembly.Memory;
let barrierDataBase = 0;

// Helper to read a string from WASM memory
function readString(ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    const copy = new Uint8Array(bytes);
    return new TextDecoder().decode(copy);
}

// Barrier wait implementation using Atomics (same as main_worker)
function barrierWait(barrierId: number): void {
    const i32 = new Int32Array(memory.buffer);
    // Barrier layout: [count, generation, arrived] - 3 i32s per barrier
    const baseIndex = (barrierDataBase / 4) + barrierId * 3;
    const countIndex = baseIndex + 0;
    const genIndex = baseIndex + 1;
    const arrivedIndex = baseIndex + 2;

    const count = Atomics.load(i32, countIndex);
    const myGen = Atomics.load(i32, genIndex);

    // Atomically increment arrived count
    const arrived = Atomics.add(i32, arrivedIndex, 1) + 1;

    if (arrived === count) {
        // Last thread: reset arrived, flip generation, wake everyone
        Atomics.store(i32, arrivedIndex, 0);
        Atomics.store(i32, genIndex, 1 - myGen);
        Atomics.notify(i32, genIndex);
    } else {
        // Wait for generation to change
        Atomics.wait(i32, genIndex, myGen);
    }
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
                js_barrier_wait: (barrierId: number) => {
                    barrierWait(barrierId);
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
    } else if (cmd === "set_barrier_base") {
        // Update barrier base address
        barrierDataBase = e.data.barrierDataBase;
    } else if (cmd === "run") {
        // Phase 2: Execute the function
        const { funcPtr, argPtr, threadId, flagIndex } = e.data;
        // Update barrier base if provided with run command
        if (e.data.barrierDataBase !== undefined) {
            barrierDataBase = e.data.barrierDataBase;
        }

        try {
            const table = wasmInstance.exports.__indirect_function_table as WebAssembly.Table;
            const fn = table.get(funcPtr) as (arg: number) => void;

            if (!fn) {
                throw new Error(`Function at index ${funcPtr} is null`);
            }

            fn(argPtr);

            // Signal completion
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, flagIndex, 1);
            Atomics.notify(flags, flagIndex);
        } catch (err) {
            console.error(`[Thread ${threadId}] Error:`, err);
            // Signal completion with error
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, flagIndex, -1);
            Atomics.notify(flags, flagIndex);
        }
    }
};
