// Main worker - runs WASM main() and can use Atomics.wait

import { OS_CORES, barrierWait, createLogImports, createWasiImports } from "./shared.ts";
import { createRenderer, createGpuImports, resizeRenderer, Renderer } from "./renderer.ts";
import { createFileSystemFunctions } from "./os_fs.ts";

// Renderer state - initialized when we receive canvas from main thread
let renderer: Renderer | null = null;
let canvasReady: Promise<{ canvas: OffscreenCanvas; dpr: number }>;
let resolveCanvas: (data: { canvas: OffscreenCanvas; dpr: number }) => void;

// Set up promise to wait for canvas
canvasReady = new Promise((resolve) => {
    resolveCanvas = resolve;
});

// Current DPR (updated on resize)
let currentDpr = 1;

// Input event types (must match C enum)
const InputEventType = {
    INPUT_EVENT_KEYDOWN: 0,
    INPUT_EVENT_KEYUP: 1,
    INPUT_EVENT_TOUCH_START: 2,
    INPUT_EVENT_TOUCH_END: 3,
    INPUT_EVENT_TOUCH_MOVE: 4,
    INPUT_EVENT_SCROLL: 5,
} as const;

// Input event buffer (accumulated between frames)
interface GameInputEvent {
    type: number;
    keyType?: number;
    touchId?: number;
    touchX?: number;
    touchY?: number;
    scrollDeltaX?: number;
    scrollDeltaY?: number;
}

const MAX_INPUT_EVENTS = 20;
const inputBuffer = {
    mouseX: 0,
    mouseY: 0,
    events: [] as GameInputEvent[],
};

// Listen for messages from main thread
self.addEventListener("message", (e) => {
    if (e.data.type === "init" && e.data.canvas) {
        currentDpr = e.data.dpr ?? 1;
        resolveCanvas({
            canvas: e.data.canvas,
            dpr: currentDpr,
        });
    } else if (e.data.type === "resize") {
        // Update canvas dimensions on resize
        if (canvasRef) {
            canvasRef.width = e.data.width;
            canvasRef.height = e.data.height;
            currentDpr = e.data.dpr ?? currentDpr;
            // Recreate depth texture to match new canvas size
            resizeRenderer(e.data.width, e.data.height);
        }
    } else if (e.data.type === "input") {
        // Accumulate input events from main thread
        inputBuffer.mouseX = e.data.mouseX;
        inputBuffer.mouseY = e.data.mouseY;
        for (const event of e.data.events) {
            if (inputBuffer.events.length < MAX_INPUT_EVENTS) {
                inputBuffer.events.push(event);
            }
        }
    }
});

// Reference to canvas for resize handling
let canvasRef: OffscreenCanvas | null = null;

// Shared memory (4GB max)
const memory = new WebAssembly.Memory({
    initial: 65536, // 16MB initial
    maximum: 65536, // 4GB max (65536 * 64KB)
    shared: true,
});

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
const POOL_SIZE = OS_CORES + 4;

// Helper to cleanup a thread (return worker to pool)
function cleanupThread(threadId: number): void {
    const thread = threads.get(threadId);
    if (!thread) return;

    // Return worker to pool
    const poolInfo = workerPool.find((w) => w.worker === thread.worker);
    if (poolInfo) {
        poolInfo.ready = true;
    }

    threads.delete(threadId);
}

// Preload worker pool
async function preloadWorker(index: number): Promise<WorkerInfo> {
    const worker = new Worker(new URL("./worker.mjs", import.meta.url), {
        type: "module",
        name: `Thread ${index}`,
    });

    return new Promise((resolve) => {
        worker.onmessage = (e) => {
            if (e.data.cmd === "loaded") {
                const info: WorkerInfo = { worker, ready: true };
                resolve(info);
            } else if (e.data.cmd === "cleanupThread") {
                // Detached thread finished - return worker to pool
                cleanupThread(e.data.threadId);
            }
        };

        worker.onerror = (e) => {
            console.error("[Main] Worker preload error:", e);
        };

        worker.postMessage({
            cmd: "load",
            index,
            wasmModule,
            memory,
        });
    });
}

// Preload all workers before running main
for (let i = 0; i < POOL_SIZE; i++) {
    const info = await preloadWorker(i);
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

// Imports for WASM - js_barrier_wait needs access to barrierDataBase which is set later,
// but the function closes over the variable so it will use the updated value
const imports = {
    env: {
        memory,
        ...createLogImports(memory, "Main Thread"),
        ...createGpuImports(memory),
        ...createFileSystemFunctions(memory),
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
        js_thread_cleanup: (threadId: number) => {
            // Called from C when detach is called after thread already exited
            cleanupThread(threadId);
        },
        js_barrier_wait: (barrierId: number) => {
            barrierWait(memory, barrierDataBase, barrierId);
        },
    },
    wasi_snapshot_preview1: createWasiImports(),
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

// Initialize renderer BEFORE wasm_main so GPU is ready
const { canvas, dpr } = await canvasReady;
canvasRef = canvas;  // Store reference for resize handling
currentDpr = dpr;
renderer = await createRenderer(canvas);
console.log("[Main Worker] WebGPU renderer initialized");

const wasm_main = instance.exports.wasm_main as (memory: number) => number;
const wasm_frame = instance.exports.wasm_frame as ((memory: number) => void) | undefined;

const heap = (instance.exports.os_get_heap_base as () => number)();

// AppMemory struct layout (360 bytes):
// offset 0:   dt (f32)
// offset 4:   total_time (f32)
// offset 8:   canvas_width (f32)
// offset 12:  canvas_height (f32)
// offset 16:  dpr (f32)
// offset 20:  input_events.mouse_x (f32)
// offset 24:  input_events.mouse_y (f32)
// offset 28:  input_events.len (u32)
// offset 32:  input_events.events[0..19] (320 bytes = 20 * 16)
// offset 352: heap pointer (u32)
// offset 356: heap_size (u32)
const APP_MEMORY_SIZE = 360;
const TOTAL_PAGES = 65536; // 4GB
const TOTAL_HEAP_SIZE = TOTAL_PAGES * 65536; // 4GB

// Offsets for input events
const INPUT_MOUSE_X_OFFSET = 20;
const INPUT_MOUSE_Y_OFFSET = 24;
const INPUT_LEN_OFFSET = 28;
const INPUT_EVENTS_OFFSET = 32;
const INPUT_EVENT_SIZE = 16; // Each AppInputEvent is 16 bytes

function writeAppMemoryFrameData(
    heapBase: number,
    dt: number,
    totalTime: number,
    canvasWidth: number,
    canvasHeight: number,
    dpr: number,
): void {
    const view = new DataView(memory.buffer);
    view.setFloat32(heapBase + 0, dt, true);
    view.setFloat32(heapBase + 4, totalTime, true);
    view.setFloat32(heapBase + 8, canvasWidth, true);
    view.setFloat32(heapBase + 12, canvasHeight, true);
    view.setFloat32(heapBase + 16, dpr, true);
}

// Initialize AppMemory struct
{
    const view = new DataView(memory.buffer);
    writeAppMemoryFrameData(heap, 0, 0, canvas.width, canvas.height, currentDpr);
    view.setUint32(heap + 352, heap + APP_MEMORY_SIZE, true); // heap pointer
    view.setUint32(heap + 356, TOTAL_HEAP_SIZE - APP_MEMORY_SIZE, true); // heap_size
}

const result = wasm_main(heap);

// Render loop - call WASM frame function if it exists
if (wasm_frame) {
    const targetFPS = 60;
    const targetFrameTimeMs = 1000 / targetFPS;
    const sleepToleranceMs = 3;
    const loopSleepToleranceMs = 1.5;

    const time = {
        startMs: performance.now(),
        nowMs: performance.now(),
        lastMs: performance.now(),
        dt: 0,
    };

    let isRunning = true;

    function tick(): void {
        if (!isRunning) return;

        // Frame timing - early exit if we're ahead of schedule
        {
            let startFrameMs = performance.now();
            let lastStartFrameMs = time.nowMs;
            let lastFrameDtMs = startFrameMs - lastStartFrameMs;

            // If way ahead of schedule, skip this frame via rAF
            if (lastFrameDtMs < targetFrameTimeMs - sleepToleranceMs) {
                requestAnimationFrame(tick);
                return;
            }

            // Spin loop for finer granularity timing
            while (lastFrameDtMs < targetFrameTimeMs - loopSleepToleranceMs) {
                startFrameMs = performance.now();
                lastFrameDtMs = startFrameMs - lastStartFrameMs;
            }
        }

        // Update time
        time.nowMs = performance.now();
        time.dt = time.nowMs - time.lastMs;
        time.lastMs = time.nowMs;

        // Calculate total time since start (in seconds)
        const totalTimeSec = (time.nowMs - time.startMs) / 1000;
        const dtSec = time.dt / 1000;

        writeAppMemoryFrameData(heap, dtSec, totalTimeSec, canvas.width, canvas.height, currentDpr);

        // Write input events to AppMemory struct
        const view = new DataView(memory.buffer);
        view.setFloat32(heap + INPUT_MOUSE_X_OFFSET, inputBuffer.mouseX, true);
        view.setFloat32(heap + INPUT_MOUSE_Y_OFFSET, inputBuffer.mouseY, true);
        view.setUint32(heap + INPUT_LEN_OFFSET, inputBuffer.events.length, true);

        // Write each input event (16 bytes each)
        for (let i = 0; i < inputBuffer.events.length && i < MAX_INPUT_EVENTS; i++) {
            const event = inputBuffer.events[i];
            const eventOffset = heap + INPUT_EVENTS_OFFSET + i * INPUT_EVENT_SIZE;

            view.setUint32(eventOffset + 0, event.type, true); // type

            if (event.type === InputEventType.INPUT_EVENT_KEYDOWN ||
                event.type === InputEventType.INPUT_EVENT_KEYUP) {
                view.setUint32(eventOffset + 4, event.keyType ?? 0, true); // key.type
                // bytes 8-15 are padding
            } else if (event.type === InputEventType.INPUT_EVENT_TOUCH_START ||
                       event.type === InputEventType.INPUT_EVENT_TOUCH_END ||
                       event.type === InputEventType.INPUT_EVENT_TOUCH_MOVE) {
                view.setUint32(eventOffset + 4, event.touchId ?? 0, true);  // touch.id
                view.setFloat32(eventOffset + 8, event.touchX ?? 0, true);  // touch.x
                view.setFloat32(eventOffset + 12, event.touchY ?? 0, true); // touch.y
            } else if (event.type === InputEventType.INPUT_EVENT_SCROLL) {
                view.setFloat32(eventOffset + 4, event.scrollDeltaX ?? 0, true);  // scroll.delta_x
                view.setFloat32(eventOffset + 8, event.scrollDeltaY ?? 0, true);  // scroll.delta_y
                // bytes 12-15 are padding
            }
        }

        // Clear input buffer after writing
        inputBuffer.events = [];

        // Call WASM frame with pointer to AppMemory struct
        wasm_frame!(heap);

        requestAnimationFrame(tick);
    }

    console.log("[Main Worker] Starting game loop at 60 FPS");
    requestAnimationFrame(tick);
}

// Notify main thread
self.postMessage({ type: "done", result });
