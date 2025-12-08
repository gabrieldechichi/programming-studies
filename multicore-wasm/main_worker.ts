// Main worker - runs WASM main() and can use Atomics.wait

// Shared memory (16MB)
const memory = new WebAssembly.Memory({
    initial: 256,
    maximum: 256,
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
const POOL_SIZE = 4;

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
    doneOffset: number;
}
const threads = new Map<number, ThreadInfo>();
let nextThreadId = 1;

// Reserve first 1KB for thread flags
const THREAD_FLAGS_BASE = 0;
const THREAD_FLAG_SIZE = 4;

// Imports for WASM
const imports = {
    env: {
        memory,
        js_log: (ptr: number, len: number) => {
            const str = readString(ptr, len);
            console.log(str);
        },
        js_thread_spawn: (funcPtr: number, argPtr: number): number => {
            // Get a ready worker from pool
            const info = workerPool.find((w) => w.ready);
            if (!info) {
                console.error("No available workers in pool!");
                return -1;
            }
            info.ready = false;

            const threadId = nextThreadId++;
            const doneOffset = THREAD_FLAGS_BASE + threadId * THREAD_FLAG_SIZE;

            // Initialize done flag to 0
            const flags = new Int32Array(memory.buffer);
            Atomics.store(flags, doneOffset / 4, 0);

            // Send run command - worker is already loaded
            info.worker.postMessage({
                cmd: "run",
                funcPtr,
                argPtr,
                threadId,
                doneOffset,
            });

            threads.set(threadId, { worker: info.worker, doneOffset });
            return threadId;
        },
        js_thread_join: (threadId: number) => {
            const thread = threads.get(threadId);
            if (!thread) return;

            const flags = new Int32Array(memory.buffer);
            const flagIndex = thread.doneOffset / 4;

            // Wait for thread to complete
            while (Atomics.load(flags, flagIndex) === 0) {
                Atomics.wait(flags, flagIndex, 0);
            }

            // Return worker to pool
            const poolInfo = workerPool.find((w) => w.worker === thread.worker);
            if (poolInfo) {
                poolInfo.ready = true;
            }

            threads.delete(threadId);
        },
    },
};

// Instantiate and run
// Note: WebAssembly.instantiate with a Module returns Instance directly (not {module, instance})
const instance = await WebAssembly.instantiate(wasmModule, imports);
const main = instance.exports.main as () => number;
const result = main();

// Notify main thread
self.postMessage({ type: "done", result });
