// Main thread just spawns a worker to run main()
// This allows the worker to use Atomics.wait for thread joins

// Input button constants (must match C enum in input.h)
const InputButton = {
    KEY_A: 0, KEY_B: 1, KEY_C: 2, KEY_D: 3, KEY_E: 4, KEY_F: 5, KEY_G: 6, KEY_H: 7,
    KEY_I: 8, KEY_J: 9, KEY_K: 10, KEY_L: 11, KEY_M: 12, KEY_N: 13, KEY_O: 14, KEY_P: 15,
    KEY_Q: 16, KEY_R: 17, KEY_S: 18, KEY_T: 19, KEY_U: 20, KEY_V: 21, KEY_W: 22, KEY_X: 23,
    KEY_Y: 24, KEY_Z: 25,
    KEY_0: 26, KEY_1: 27, KEY_2: 28, KEY_3: 29, KEY_4: 30, KEY_5: 31, KEY_6: 32, KEY_7: 33,
    KEY_8: 34, KEY_9: 35,
    KEY_F1: 36, KEY_F2: 37, KEY_F3: 38, KEY_F4: 39, KEY_F5: 40, KEY_F6: 41,
    KEY_F7: 42, KEY_F8: 43, KEY_F9: 44, KEY_F10: 45, KEY_F11: 46, KEY_F12: 47,
    KEY_UP: 48, KEY_DOWN: 49, KEY_LEFT: 50, KEY_RIGHT: 51,
    KEY_SPACE: 52, KEY_ENTER: 53, KEY_ESCAPE: 54, KEY_TAB: 55, KEY_BACKSPACE: 56,
    KEY_DELETE: 57, KEY_INSERT: 58, KEY_HOME: 59, KEY_END: 60,
    KEY_PAGE_UP: 61, KEY_PAGE_DOWN: 62,
    KEY_LEFT_SHIFT: 63, KEY_RIGHT_SHIFT: 64,
    KEY_LEFT_CONTROL: 65, KEY_RIGHT_CONTROL: 66,
    KEY_LEFT_ALT: 67, KEY_RIGHT_ALT: 68,
    MOUSE_LEFT: 69, MOUSE_RIGHT: 70, MOUSE_MIDDLE: 71,
} as const;

const InputEventType = {
    INPUT_EVENT_KEYDOWN: 0,
    INPUT_EVENT_KEYUP: 1,
    INPUT_EVENT_TOUCH_START: 2,
    INPUT_EVENT_TOUCH_END: 3,
    INPUT_EVENT_TOUCH_MOVE: 4,
    INPUT_EVENT_SCROLL: 5,
} as const;

// Map browser key codes to InputButton values
const keyCodeMap = new Map<string, number>([
    ["KeyA", InputButton.KEY_A], ["KeyB", InputButton.KEY_B], ["KeyC", InputButton.KEY_C],
    ["KeyD", InputButton.KEY_D], ["KeyE", InputButton.KEY_E], ["KeyF", InputButton.KEY_F],
    ["KeyG", InputButton.KEY_G], ["KeyH", InputButton.KEY_H], ["KeyI", InputButton.KEY_I],
    ["KeyJ", InputButton.KEY_J], ["KeyK", InputButton.KEY_K], ["KeyL", InputButton.KEY_L],
    ["KeyM", InputButton.KEY_M], ["KeyN", InputButton.KEY_N], ["KeyO", InputButton.KEY_O],
    ["KeyP", InputButton.KEY_P], ["KeyQ", InputButton.KEY_Q], ["KeyR", InputButton.KEY_R],
    ["KeyS", InputButton.KEY_S], ["KeyT", InputButton.KEY_T], ["KeyU", InputButton.KEY_U],
    ["KeyV", InputButton.KEY_V], ["KeyW", InputButton.KEY_W], ["KeyX", InputButton.KEY_X],
    ["KeyY", InputButton.KEY_Y], ["KeyZ", InputButton.KEY_Z],
    ["Digit0", InputButton.KEY_0], ["Digit1", InputButton.KEY_1], ["Digit2", InputButton.KEY_2],
    ["Digit3", InputButton.KEY_3], ["Digit4", InputButton.KEY_4], ["Digit5", InputButton.KEY_5],
    ["Digit6", InputButton.KEY_6], ["Digit7", InputButton.KEY_7], ["Digit8", InputButton.KEY_8],
    ["Digit9", InputButton.KEY_9],
    ["F1", InputButton.KEY_F1], ["F2", InputButton.KEY_F2], ["F3", InputButton.KEY_F3],
    ["F4", InputButton.KEY_F4], ["F5", InputButton.KEY_F5], ["F6", InputButton.KEY_F6],
    ["F7", InputButton.KEY_F7], ["F8", InputButton.KEY_F8], ["F9", InputButton.KEY_F9],
    ["F10", InputButton.KEY_F10], ["F11", InputButton.KEY_F11], ["F12", InputButton.KEY_F12],
    ["ArrowUp", InputButton.KEY_UP], ["ArrowDown", InputButton.KEY_DOWN],
    ["ArrowLeft", InputButton.KEY_LEFT], ["ArrowRight", InputButton.KEY_RIGHT],
    ["Space", InputButton.KEY_SPACE], ["Enter", InputButton.KEY_ENTER],
    ["Escape", InputButton.KEY_ESCAPE], ["Tab", InputButton.KEY_TAB],
    ["Backspace", InputButton.KEY_BACKSPACE], ["Delete", InputButton.KEY_DELETE],
    ["Insert", InputButton.KEY_INSERT], ["Home", InputButton.KEY_HOME], ["End", InputButton.KEY_END],
    ["PageUp", InputButton.KEY_PAGE_UP], ["PageDown", InputButton.KEY_PAGE_DOWN],
    ["ShiftLeft", InputButton.KEY_LEFT_SHIFT], ["ShiftRight", InputButton.KEY_RIGHT_SHIFT],
    ["ControlLeft", InputButton.KEY_LEFT_CONTROL], ["ControlRight", InputButton.KEY_RIGHT_CONTROL],
    ["AltLeft", InputButton.KEY_LEFT_ALT], ["AltRight", InputButton.KEY_RIGHT_ALT],
]);

const mouseButtonMap = [InputButton.MOUSE_LEFT, InputButton.MOUSE_MIDDLE, InputButton.MOUSE_RIGHT];

// Input event types for posting to worker
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
const inputState = {
    mouseX: 0,
    mouseY: 0,
    events: [] as GameInputEvent[],
};

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

// Input event handlers
function handleKeyDown(event: KeyboardEvent): void {
    const button = keyCodeMap.get(event.code);
    if (button !== undefined && inputState.events.length < MAX_INPUT_EVENTS) {
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_KEYDOWN,
            keyType: button,
        });
    }
}

function handleKeyUp(event: KeyboardEvent): void {
    const button = keyCodeMap.get(event.code);
    if (button !== undefined && inputState.events.length < MAX_INPUT_EVENTS) {
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_KEYUP,
            keyType: button,
        });
    }
}

function handleMouseMove(event: MouseEvent): void {
    if (document.pointerLockElement === canvas) {
        inputState.mouseX += event.movementX;
        inputState.mouseY += event.movementY;
    } else {
        const rect = canvas.getBoundingClientRect();
        inputState.mouseX = event.clientX - rect.left;
        inputState.mouseY = event.clientY - rect.top;
    }
}

function handleMouseDown(event: MouseEvent): void {
    const button = mouseButtonMap[event.button];
    if (button !== undefined && inputState.events.length < MAX_INPUT_EVENTS) {
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_KEYDOWN,
            keyType: button,
        });
    }
}

function handleMouseUp(event: MouseEvent): void {
    const button = mouseButtonMap[event.button];
    if (button !== undefined && inputState.events.length < MAX_INPUT_EVENTS) {
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_KEYUP,
            keyType: button,
        });
    }
}

function handleTouchStart(event: TouchEvent): void {
    event.preventDefault();
    for (let i = 0; i < event.changedTouches.length && inputState.events.length < MAX_INPUT_EVENTS; i++) {
        const touch = event.changedTouches[i];
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_TOUCH_START,
            touchId: touch.identifier,
            touchX: touch.clientX,
            touchY: touch.clientY,
        });
    }
}

function handleTouchEnd(event: TouchEvent): void {
    event.preventDefault();
    for (let i = 0; i < event.changedTouches.length && inputState.events.length < MAX_INPUT_EVENTS; i++) {
        const touch = event.changedTouches[i];
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_TOUCH_END,
            touchId: touch.identifier,
            touchX: touch.clientX,
            touchY: touch.clientY,
        });
    }
}

function handleTouchMove(event: TouchEvent): void {
    event.preventDefault();
    for (let i = 0; i < event.changedTouches.length && inputState.events.length < MAX_INPUT_EVENTS; i++) {
        const touch = event.changedTouches[i];
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_TOUCH_MOVE,
            touchId: touch.identifier,
            touchX: touch.clientX,
            touchY: touch.clientY,
        });
    }
}

function handleWheel(event: WheelEvent): void {
    if (inputState.events.length < MAX_INPUT_EVENTS) {
        inputState.events.push({
            type: InputEventType.INPUT_EVENT_SCROLL,
            scrollDeltaX: event.deltaX,
            scrollDeltaY: event.deltaY,
        });
    }
}

function handleContextMenu(event: MouseEvent): boolean {
    event.preventDefault();
    event.stopPropagation();
    return false;
}

// Register event listeners
window.addEventListener("keydown", handleKeyDown);
window.addEventListener("keyup", handleKeyUp);
canvas.addEventListener("mousedown", handleMouseDown);
canvas.addEventListener("mouseup", handleMouseUp);
canvas.addEventListener("mousemove", handleMouseMove);
canvas.addEventListener("contextmenu", handleContextMenu);
canvas.addEventListener("touchstart", handleTouchStart, { passive: false });
canvas.addEventListener("touchend", handleTouchEnd, { passive: false });
canvas.addEventListener("touchmove", handleTouchMove, { passive: false });
canvas.addEventListener("wheel", handleWheel);

// Send input to worker periodically (sync with requestAnimationFrame)
function sendInputToWorker(): void {
    if (inputState.events.length > 0 || inputState.mouseX !== 0 || inputState.mouseY !== 0) {
        worker.postMessage({
            type: "input",
            mouseX: inputState.mouseX,
            mouseY: inputState.mouseY,
            events: inputState.events,
        });
        inputState.events = [];
    }
    requestAnimationFrame(sendInputToWorker);
}
requestAnimationFrame(sendInputToWorker);
