// Main worker - runs WASM main() and can use Atomics.wait

// Shared memory (4GB max)
const memory = new WebAssembly.Memory({
    initial: 65536,      // 16MB initial
    maximum: 65536,    // 4GB max (65536 * 64KB)
    shared: true,
});

// Helper to read a string from WASM memory
function readString(ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    const copy = new Uint8Array(bytes);
    return new TextDecoder().decode(copy);
}

// Load and compile WASM once
const wasmUrl = new URL("./wasm.wasm", import.meta.url);
const wasmResponse = await fetch(wasmUrl);
const wasmBytes = await wasmResponse.arrayBuffer();
const wasmModule = await WebAssembly.compile(wasmBytes);

// Worker pool - preloaded and ready
interface WorkerInfo {
    worker: Worker;
    ready: boolean;
}
const workerPool: WorkerInfo[] = [];
const hardwareCores = navigator.hardwareConcurrency;
const OS_CORES = hardwareCores < 8 ? 16 : hardwareCores;
const POOL_SIZE = OS_CORES + 4;

// Preload worker pool
async function preloadWorker(): Promise<WorkerInfo> {
    const worker = new Worker(new URL("./worker.mjs", import.meta.url), {
        type: "module",
        name: "wasm-thread",
    });

    return new Promise((resolve) => {
        worker.onmessage = (e) => {
            if (e.data.cmd === "loaded") {
                const info: WorkerInfo = { worker, ready: true };
                resolve(info);
            }
        };

        worker.onerror = (e) => {
            console.error("[Main] Worker preload error:", e);
        };

        worker.postMessage({
            cmd: "load",
            wasmModule,
            memory,
        });
    });
}

// Preload all workers before running main
for (let i = 0; i < POOL_SIZE; i++) {
    const info = await preloadWorker();
    workerPool.push(info);
}

// Thread tracking
interface ThreadInfo {
    worker: Worker;
    flagIndex: number; // index into Int32Array (not byte offset)
}
const threads = new Map<number, ThreadInfo>();
let nextThreadId = 1;

// These will be set after WASM instantiation from exported globals
let threadFlagsBase = 0; // byte offset of thread_flags array
let barrierDataBase = 0; // byte offset of barrier_data array

// TLS (Thread Local Storage) support - managed by C side

// Barrier wait implementation using Atomics
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

// Imports for WASM - js_barrier_wait needs access to barrierDataBase which is set later,
// but the function closes over the variable so it will use the updated value
const imports = {
    env: {
        memory,
        js_log: (ptr: number, len: number) => {
            const str = readString(ptr, len);
            console.log(str);
        },
        _os_log_info: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ): void => {
            const message = readString(ptr, len);
            const fileName = readString(fileNamePtr, fileNameLen);
            console.log(`${fileName}:${lineNumber}: ${message}`);
        },
        _os_log_warn: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ): void => {
            const message = readString(ptr, len);
            const fileName = readString(fileNamePtr, fileNameLen);
            console.warn(`${fileName}:${lineNumber}: ${message}`);
        },
        _os_log_error: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ): void => {
            const message = readString(ptr, len);
            const fileName = readString(fileNamePtr, fileNameLen);
            console.error(`${fileName}:${lineNumber}: ${message}`);
        },
        js_get_core_count: () => {
            return OS_CORES;
        },
        js_thread_spawn: (
            funcPtr: number,
            argPtr: number,
            stackTop: number,
            tlsBase: number,
        ): number => {
            // Get a ready worker from pool
            const info = workerPool.find((w) => w.ready);
            if (!info) {
                console.error("No available workers in pool!");
                return -1;
            }
            info.ready = false;

            const threadId = nextThreadId++;
            // Use exported thread_flags array - each thread gets one i32 slot
            const flagIndex = threadFlagsBase / 4 + threadId;

            // Initialize done flag to 0
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, flagIndex, 0);

            // Send run command - worker is already loaded
            // stackTop and tlsBase come from C (allocated by os_thread_launch)
            info.worker.postMessage({
                cmd: "run",
                funcPtr,
                argPtr,
                threadId,
                flagIndex,
                barrierDataBase,
                stackTop,
                tlsBase,
            });

            threads.set(threadId, { worker: info.worker, flagIndex });
            return threadId;
        },
        js_thread_join: (threadId: number) => {
            const thread = threads.get(threadId);
            if (!thread) return;

            const flags = new Int32Array(memory.buffer);

            // Wait for thread to complete
            while (Atomics.load(flags, thread.flagIndex) === 0) {
                Atomics.wait(flags, thread.flagIndex, 0);
            }

            // Return worker to pool
            const poolInfo = workerPool.find((w) => w.worker === thread.worker);
            if (poolInfo) {
                poolInfo.ready = true;
            }

            threads.delete(threadId);
        },
        js_barrier_wait: (barrierId: number) => {
            barrierWait(barrierId);
        },
    },
};

// Instantiate and run
const instance = await WebAssembly.instantiate(wasmModule, imports);

// Get addresses for thread sync data via getter functions
const get_thread_flags_ptr = instance.exports.get_thread_flags_ptr as () => number;
const get_barrier_data_ptr = instance.exports.get_barrier_data_ptr as () => number;
threadFlagsBase = get_thread_flags_ptr();
barrierDataBase = get_barrier_data_ptr();

// Initialize TLS for main worker (uses slot 0, managed by C side)
const __tls_size = instance.exports.__tls_size as WebAssembly.Global;
const tlsSize = __tls_size.value as number;
if (tlsSize > 0) {
    const get_tls_slot_base = instance.exports.get_tls_slot_base as (slot: number) => number;
    const __wasm_init_tls = instance.exports.__wasm_init_tls as (ptr: number) => void;
    const mainTlsBase = get_tls_slot_base(0); // Main worker uses slot 0
    __wasm_init_tls(mainTlsBase);
}

// Send barrier base and TLS info to all workers so they can do barrier_wait
for (const info of workerPool) {
    info.worker.postMessage({
        cmd: "set_barrier_base",
        barrierDataBase,
    });
}

const wasm_main = instance.exports.wasm_main as () => number;
const result = wasm_main();

// Notify main thread
self.postMessage({ type: "done", result });
