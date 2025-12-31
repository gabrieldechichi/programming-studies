// Worker thread - receives WASM module and executes functions

import { barrierWait, createLogImports, createWasiImports } from "./shared.ts";
import { createGpuImports } from "./renderer.ts";
import { createFileSystemFunctions } from "./os_fs.ts";

// State set during 'load' phase
let wasmInstance: WebAssembly.Instance;
let memory: WebAssembly.Memory;
let threadIdx: number;
let barrierDataBase = 0;

self.onmessage = async (e) => {
    const { cmd } = e.data;

    if (cmd === "load") {
        // Phase 1: Receive module and memory, instantiate WASM
        threadIdx = e.data.index;
        memory = e.data.memory;
        const wasmModule = e.data.wasmModule as WebAssembly.Module;

        const imports = {
            env: {
                memory,
                ...createLogImports(memory, `Thread ${threadIdx}`),
                ...createGpuImports(memory),
                ...createFileSystemFunctions(memory),
                // Workers can't spawn threads (for now)
                js_thread_spawn: () => {
                    console.error("Cannot spawn threads from worker");
                    return -1;
                },
                js_thread_join: () => {
                    console.error("Cannot join threads from worker");
                },
                js_thread_cleanup: () => {
                    // Workers don't directly cleanup - they post a message to main_worker
                    // This is a stub to satisfy the WASM import
                },
                js_barrier_wait: (barrierId: number) => {
                    barrierWait(memory, barrierDataBase, barrierId);
                },
            },
            wasi_snapshot_preview1: createWasiImports(),
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
        const { funcPtr, argPtr, threadId, flagIndex, stackTop, tlsBase } = e.data;
        // Update barrier base if provided with run command
        if (e.data.barrierDataBase !== undefined) {
            barrierDataBase = e.data.barrierDataBase;
        }

        try {
            // Set __stack_pointer directly via the exported global BEFORE any WASM call.
            // Each worker instance starts with __stack_pointer pointing to the main thread's stack.
            // Calling any WASM function before setting this corrupts the main thread's stack.
            const stackPointerGlobal = wasmInstance.exports.__stack_pointer as WebAssembly.Global;
            stackPointerGlobal.value = stackTop;

            // Initialize TLS for this worker
            if (tlsBase !== undefined && tlsBase !== 0) {
                const __wasm_init_tls = wasmInstance.exports.__wasm_init_tls as (ptr: number) => void;
                __wasm_init_tls(tlsBase);
            }

            const table = wasmInstance.exports.__indirect_function_table as WebAssembly.Table;
            const fn = table.get(funcPtr) as (arg: number) => void;

            if (!fn) {
                throw new Error(`Function at index ${funcPtr} is null`);
            }

            fn(argPtr);

            // Check detach state and do cleanup if needed
            const os_thread_exit_check = wasmInstance.exports.os_thread_exit_check as (
                id: number,
            ) => number;
            const needsCleanup = os_thread_exit_check(threadId);

            if (needsCleanup) {
                // Was detached - tell main_worker to return this worker to pool
                self.postMessage({ cmd: "cleanupThread", threadId });
            }

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
