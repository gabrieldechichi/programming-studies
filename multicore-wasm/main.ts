// Main thread just spawns a worker to run main()
// This allows the worker to use Atomics.wait for thread joins

const worker = new Worker(new URL("./main_worker.mjs", import.meta.url), {
    type: "module",
    name: "Main Thread",
});

worker.onmessage = (e) => {
    if (e.data.type === "done") {
        console.log(`main() returned: ${e.data.result}`);
    }
};

worker.onerror = (e) => {
    console.error("Worker error:", e);
};
