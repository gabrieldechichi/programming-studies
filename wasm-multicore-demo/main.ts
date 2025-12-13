// Main thread just spawns a worker to run main()
// This allows the worker to use Atomics.wait for thread joins

const canvas = document.getElementById("canvas") as HTMLCanvasElement;
if (!canvas) {
    throw new Error("Canvas element not found");
}

// Transfer canvas control to worker for WebGPU rendering
const offscreen = canvas.transferControlToOffscreen();
offscreen.width = canvas.clientWidth * devicePixelRatio;
offscreen.height = canvas.clientHeight * devicePixelRatio;

const worker = new Worker(new URL("./main_worker.mjs", import.meta.url), {
    type: "module",
    name: "Main Thread",
});

// Send canvas and DPR to worker
worker.postMessage({ type: "init", canvas: offscreen, dpr: devicePixelRatio }, [offscreen]);

// Handle window resize - notify worker of new dimensions
window.addEventListener("resize", () => {
    const width = canvas.clientWidth * devicePixelRatio;
    const height = canvas.clientHeight * devicePixelRatio;
    worker.postMessage({ type: "resize", width, height, dpr: devicePixelRatio });
});

worker.onmessage = (e) => {
    if (e.data.type === "done") {
        console.log(`main() returned: ${e.data.result}`);
    }
};

worker.onerror = (e) => {
    console.error("Worker error:", e);
};
