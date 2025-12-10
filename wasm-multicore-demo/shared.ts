// Shared utilities for WASM threading

export const hardwareCores = navigator.hardwareConcurrency;
export const OS_CORES = hardwareCores < 8 ? 16 : hardwareCores;

export function readString(memory: WebAssembly.Memory, ptr: number, len: number): string {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    const copy = new Uint8Array(bytes);
    return new TextDecoder().decode(copy);
}

export function barrierWait(
    memory: WebAssembly.Memory,
    barrierDataBase: number,
    barrierId: number,
): void {
    const i32 = new Int32Array(memory.buffer);
    // Barrier layout: [count, generation, arrived, leaving] - 4 i32s per barrier
    const baseIndex = barrierDataBase / 4 + barrierId * 4;
    const countIndex = baseIndex + 0;
    const genIndex = baseIndex + 1;
    const arrivedIndex = baseIndex + 2;
    const leavingIndex = baseIndex + 3;

    const count = Atomics.load(i32, countIndex);

    // Phase 0: Wait for previous barrier to fully drain (leaving == 0)
    // Use single load to avoid race between condition check and wait argument
    let leaving: number;
    while ((leaving = Atomics.load(i32, leavingIndex)) !== 0) {
        Atomics.wait(i32, leavingIndex, leaving);
    }

    // Capture generation BEFORE incrementing arrived
    const myGen = Atomics.load(i32, genIndex);

    // Phase 1: Arrival - increment arrived count
    const arrived = Atomics.add(i32, arrivedIndex, 1) + 1;

    if (arrived === count) {
        // Last thread to arrive:
        // - Reset arrived for next barrier use
        // - Set leaving = count (all threads need to exit)
        // - Flip generation to release waiters
        Atomics.store(i32, arrivedIndex, 0);
        Atomics.store(i32, leavingIndex, count);
        Atomics.store(i32, genIndex, 1 - myGen);
        // Wake all threads waiting on generation
        Atomics.notify(i32, genIndex);
    } else {
        // Wait for generation to change
        while (Atomics.load(i32, genIndex) === myGen) {
            Atomics.wait(i32, genIndex, myGen);
        }
    }

    // Phase 2: Exit - decrement leaving count
    const stillLeaving = Atomics.sub(i32, leavingIndex, 1) - 1;
    if (stillLeaving === 0) {
        // Last thread to leave: wake anyone waiting to enter next barrier
        Atomics.notify(i32, leavingIndex);
    }
}

export interface LogImports {
    js_log: (ptr: number, len: number) => void;
    _os_log_info: (
        ptr: number,
        len: number,
        fileNamePtr: number,
        fileNameLen: number,
        lineNumber: number,
    ) => void;
    _os_log_warn: (
        ptr: number,
        len: number,
        fileNamePtr: number,
        fileNameLen: number,
        lineNumber: number,
    ) => void;
    _os_log_error: (
        ptr: number,
        len: number,
        fileNamePtr: number,
        fileNameLen: number,
        lineNumber: number,
    ) => void;
    js_get_core_count: () => number;
}

// WASI stubs - minimal implementations for wasi-threads libc
export function createWasiImports() {
    return {
        args_get: () => 0,
        args_sizes_get: (argc_ptr: number, argv_buf_size_ptr: number) => {
            // No args - set both to 0
            return 0;
        },
        proc_exit: (code: number) => {
            console.log(`[WASI] proc_exit(${code})`);
        },
        sched_yield: () => 0,
    };
}

export function createLogImports(memory: WebAssembly.Memory, prefix?: string): LogImports {
    const logPrefix = prefix ? `[${prefix}] ` : "";
    return {
        js_log: (ptr: number, len: number) => {
            const str = readString(memory, ptr, len);
            console.log(`${logPrefix}${str}`);
        },
        _os_log_info: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ) => {
            const message = readString(memory, ptr, len);
            const fileName = readString(memory, fileNamePtr, fileNameLen);
            console.log(`${fileName}:${lineNumber}: ${message}`);
        },
        _os_log_warn: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ) => {
            const message = readString(memory, ptr, len);
            const fileName = readString(memory, fileNamePtr, fileNameLen);
            console.warn(`${fileName}:${lineNumber}: ${message}`);
        },
        _os_log_error: (
            ptr: number,
            len: number,
            fileNamePtr: number,
            fileNameLen: number,
            lineNumber: number,
        ) => {
            const message = readString(memory, ptr, len);
            const fileName = readString(memory, fileNamePtr, fileNameLen);
            console.error(`${fileName}:${lineNumber}: ${message}`);
        },
        js_get_core_count: () => OS_CORES,
    };
}
