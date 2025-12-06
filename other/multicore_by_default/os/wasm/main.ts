import { createFileSystemFunctions } from "./os_fs.ts";
import { createWebGLRenderer } from "./renderer.ts";
import { createAudioFunctions } from "./audio.ts";
import { WasmMemoryInterface } from "./wasm_memory.ts";

const canvas = document.getElementById("canvas") as HTMLCanvasElement;

const InputButton = {
  KEY_A: 0,
  KEY_B: 1,
  KEY_C: 2,
  KEY_D: 3,
  KEY_E: 4,
  KEY_F: 5,
  KEY_G: 6,
  KEY_H: 7,
  KEY_I: 8,
  KEY_J: 9,
  KEY_K: 10,
  KEY_L: 11,
  KEY_M: 12,
  KEY_N: 13,
  KEY_O: 14,
  KEY_P: 15,
  KEY_Q: 16,
  KEY_R: 17,
  KEY_S: 18,
  KEY_T: 19,
  KEY_U: 20,
  KEY_V: 21,
  KEY_W: 22,
  KEY_X: 23,
  KEY_Y: 24,
  KEY_Z: 25,
  KEY_0: 26,
  KEY_1: 27,
  KEY_2: 28,
  KEY_3: 29,
  KEY_4: 30,
  KEY_5: 31,
  KEY_6: 32,
  KEY_7: 33,
  KEY_8: 34,
  KEY_9: 35,
  KEY_F1: 36,
  KEY_F2: 37,
  KEY_F3: 38,
  KEY_F4: 39,
  KEY_F5: 40,
  KEY_F6: 41,
  KEY_F7: 42,
  KEY_F8: 43,
  KEY_F9: 44,
  KEY_F10: 45,
  KEY_F11: 46,
  KEY_F12: 47,
  KEY_UP: 48,
  KEY_DOWN: 49,
  KEY_LEFT: 50,
  KEY_RIGHT: 51,
  KEY_SPACE: 52,
  KEY_ENTER: 53,
  KEY_ESCAPE: 54,
  KEY_TAB: 55,
  KEY_BACKSPACE: 56,
  KEY_DELETE: 57,
  KEY_INSERT: 58,
  KEY_HOME: 59,
  KEY_END: 60,
  KEY_PAGE_UP: 61,
  KEY_PAGE_DOWN: 62,
  KEY_LEFT_SHIFT: 63,
  KEY_RIGHT_SHIFT: 64,
  KEY_LEFT_CONTROL: 65,
  KEY_RIGHT_CONTROL: 66,
  KEY_LEFT_ALT: 67,
  KEY_RIGHT_ALT: 68,
  MOUSE_LEFT: 69,
  MOUSE_RIGHT: 70,
  MOUSE_MIDDLE: 71,
} as const;

const InputEventType = {
  INPUT_EVENT_KEYDOWN: 0,
  INPUT_EVENT_KEYUP: 1,
  INPUT_EVENT_TOUCH_START: 2,
  INPUT_EVENT_TOUCH_END: 3,
  INPUT_EVENT_TOUCH_MOVE: 4,
  INPUT_EVENT_SCROLL: 5,
} as const;

const keyCodeToButton: [string, number][] = [
  ["KeyA", InputButton.KEY_A],
  ["KeyB", InputButton.KEY_B],
  ["KeyC", InputButton.KEY_C],
  ["KeyD", InputButton.KEY_D],
  ["KeyE", InputButton.KEY_E],
  ["KeyF", InputButton.KEY_F],
  ["KeyG", InputButton.KEY_G],
  ["KeyH", InputButton.KEY_H],
  ["KeyI", InputButton.KEY_I],
  ["KeyJ", InputButton.KEY_J],
  ["KeyK", InputButton.KEY_K],
  ["KeyL", InputButton.KEY_L],
  ["KeyM", InputButton.KEY_M],
  ["KeyN", InputButton.KEY_N],
  ["KeyO", InputButton.KEY_O],
  ["KeyP", InputButton.KEY_P],
  ["KeyQ", InputButton.KEY_Q],
  ["KeyR", InputButton.KEY_R],
  ["KeyS", InputButton.KEY_S],
  ["KeyT", InputButton.KEY_T],
  ["KeyU", InputButton.KEY_U],
  ["KeyV", InputButton.KEY_V],
  ["KeyW", InputButton.KEY_W],
  ["KeyX", InputButton.KEY_X],
  ["KeyY", InputButton.KEY_Y],
  ["KeyZ", InputButton.KEY_Z],
  ["Digit0", InputButton.KEY_0],
  ["Digit1", InputButton.KEY_1],
  ["Digit2", InputButton.KEY_2],
  ["Digit3", InputButton.KEY_3],
  ["Digit4", InputButton.KEY_4],
  ["Digit5", InputButton.KEY_5],
  ["Digit6", InputButton.KEY_6],
  ["Digit7", InputButton.KEY_7],
  ["Digit8", InputButton.KEY_8],
  ["Digit9", InputButton.KEY_9],
  ["F1", InputButton.KEY_F1],
  ["F2", InputButton.KEY_F2],
  ["F3", InputButton.KEY_F3],
  ["F4", InputButton.KEY_F4],
  ["F5", InputButton.KEY_F5],
  ["F6", InputButton.KEY_F6],
  ["F7", InputButton.KEY_F7],
  ["F8", InputButton.KEY_F8],
  ["F9", InputButton.KEY_F9],
  ["F10", InputButton.KEY_F10],
  ["F11", InputButton.KEY_F11],
  ["F12", InputButton.KEY_F12],
  ["ArrowUp", InputButton.KEY_UP],
  ["ArrowDown", InputButton.KEY_DOWN],
  ["ArrowLeft", InputButton.KEY_LEFT],
  ["ArrowRight", InputButton.KEY_RIGHT],
  ["Space", InputButton.KEY_SPACE],
  ["Enter", InputButton.KEY_ENTER],
  ["Escape", InputButton.KEY_ESCAPE],
  ["Tab", InputButton.KEY_TAB],
  ["Backspace", InputButton.KEY_BACKSPACE],
  ["Delete", InputButton.KEY_DELETE],
  ["Insert", InputButton.KEY_INSERT],
  ["Home", InputButton.KEY_HOME],
  ["End", InputButton.KEY_END],
  ["PageUp", InputButton.KEY_PAGE_UP],
  ["PageDown", InputButton.KEY_PAGE_DOWN],
  ["ShiftLeft", InputButton.KEY_LEFT_SHIFT],
  ["ShiftRight", InputButton.KEY_RIGHT_SHIFT],
  ["ControlLeft", InputButton.KEY_LEFT_CONTROL],
  ["ControlRight", InputButton.KEY_RIGHT_CONTROL],
  ["AltLeft", InputButton.KEY_LEFT_ALT],
  ["AltRight", InputButton.KEY_RIGHT_ALT],
];

const keyCodeMap = new Map(keyCodeToButton);

const mouseButtonMap = [
  InputButton.MOUSE_LEFT,
  InputButton.MOUSE_MIDDLE,
  InputButton.MOUSE_RIGHT,
];

class StructWriter {
  buffer: ArrayBuffer;
  view: DataView;
  offset: number;

  constructor(memory: WebAssembly.Memory, baseOffset: number) {
    this.buffer = memory.buffer;
    this.view = new DataView(this.buffer);
    this.offset = baseOffset;
  }

  writeF32(value: number): this {
    this.view.setFloat32(this.offset, value, true);
    this.offset += 4;
    return this;
  }

  writeU32(value: number): this {
    this.view.setUint32(this.offset, value, true);
    this.offset += 4;
    return this;
  }

  skip(bytes: number): this {
    this.offset += bytes;
    return this;
  }

  reset(baseOffset: number): this {
    this.offset = baseOffset;
    return this;
  }

  getCurrentOffset(): number {
    return this.offset;
  }
}

interface GameInputEvent {
  type: number;
  keyType?: number;
  touchId?: number;
  touchX?: number;
  touchY?: number;
  scrollDeltaX?: number;
  scrollDeltaY?: number;
}

class AppMemoryWriter {
  writer: StructWriter;
  baseOffset: number;
  permanentMemorySize: number;
  temporaryMemorySize: number;
  permanentMemoryCommitted: number;
  temporaryMemoryCommitted: number;
  permanentMemoryPtr: number;
  temporaryMemoryPtr: number;
  time: { now: number; dt: number };
  canvas: { width: number; height: number };
  inputEvents: {
    mouseX: number;
    mouseY: number;
    events: GameInputEvent[];
  };

  constructor(
    wasmMemory: WasmMemoryInterface,
    baseOffset: number,
    permanentMemorySize: number,
    temporaryMemorySize: number,
    permanentMemoryPtr: number,
    temporaryMemoryPtr: number,
  ) {
    this.writer = new StructWriter(wasmMemory.memory, baseOffset);
    this.baseOffset = baseOffset;
    this.permanentMemorySize = permanentMemorySize;
    this.temporaryMemorySize = temporaryMemorySize;
    this.permanentMemoryCommitted = permanentMemorySize;
    this.temporaryMemoryCommitted = temporaryMemorySize;
    this.permanentMemoryPtr = permanentMemoryPtr;
    this.temporaryMemoryPtr = temporaryMemoryPtr;

    this.time = {
      now: 0,
      dt: 0,
    };

    this.canvas = {
      width: 0,
      height: 0,
    };

    this.inputEvents = {
      mouseX: 0,
      mouseY: 0,
      events: [],
    };
  }

  write(): this {
    this.writer.reset(this.baseOffset);

    this.writer
      .writeF32(this.time.now)
      .writeF32(this.time.dt)
      .writeU32(this.canvas.width)
      .writeU32(this.canvas.height)
      .writeF32(this.inputEvents.mouseX)
      .writeF32(this.inputEvents.mouseY)
      .writeU32(this.inputEvents.events.length);

    const maxEvents = 20;
    for (let i = 0; i < maxEvents; i++) {
      if (i < this.inputEvents.events.length) {
        const event = this.inputEvents.events[i];
        this.writer.writeU32(event.type);

        if (
          event.type === InputEventType.INPUT_EVENT_KEYDOWN ||
          event.type === InputEventType.INPUT_EVENT_KEYUP
        ) {
          this.writer.writeU32(event.keyType!).skip(8);
        } else if (
          event.type === InputEventType.INPUT_EVENT_TOUCH_START ||
          event.type === InputEventType.INPUT_EVENT_TOUCH_END ||
          event.type === InputEventType.INPUT_EVENT_TOUCH_MOVE
        ) {
          this.writer
            .writeU32(event.touchId!)
            .writeF32(event.touchX!)
            .writeF32(event.touchY!);
        } else if (event.type === InputEventType.INPUT_EVENT_SCROLL) {
          this.writer
            .writeF32(event.scrollDeltaX!)
            .writeF32(event.scrollDeltaY!)
            .skip(4);
        } else {
          this.writer.skip(12);
        }
      } else {
        this.writer.skip(16);
      }
    }

    this.inputEvents.events.length = 0;

    this.writer
      .writeU32(0)
      .writeU32(this.permanentMemorySize)
      .writeU32(this.temporaryMemorySize)
      .writeU32(this.permanentMemoryCommitted)
      .writeU32(this.temporaryMemoryCommitted)
      .writeU32(this.permanentMemoryPtr)
      .writeU32(this.temporaryMemoryPtr);

    return this;
  }
}

interface WasmExports extends WebAssembly.Exports {
  app_init: (offset: number) => void;
  app_update_and_render: (offset: number) => void;
  app_on_reload: (offset: number) => void;
  app_memory_size: () => number;
  os_get_heap_base: () => number;
  app_ctx_current: () => number;
  app_ctx_set: (ctxPtr: number) => void;
}

interface GameState {
  memory: WebAssembly.Memory;
  wasmMemory: WasmMemoryInterface;
  appMemory: AppMemoryWriter;
  appMemoryOffset: number;
  exports: {
    app_init: (offset: number) => void;
    app_update_and_render: (offset: number) => void;
    app_on_reload: (offset: number) => void;
    app_ctx_current: () => number;
    app_ctx_set: (ctxPtr: number) => void;
  };
  renderer: ReturnType<typeof createWebGLRenderer>;
  fsFunctions: ReturnType<typeof createFileSystemFunctions>;
  audioFunctions: ReturnType<typeof createAudioFunctions>;
  platform: {
    audioContext: AudioContext | null;
    audioWorkletNode: AudioWorkletNode | null;
  };
  isRunning: boolean;
}

async function loadWasmModule(
  memory: WebAssembly.Memory,
  wasmMemory: WasmMemoryInterface,
  renderer: ReturnType<typeof createWebGLRenderer>,
  fsFunctions: ReturnType<typeof createFileSystemFunctions>,
  audioFunctions: ReturnType<typeof createAudioFunctions>,
): Promise<WasmExports> {
  const wasmPath = `./game.wasm?v=${Date.now()}`;
  const response = await fetch(wasmPath);

  if (!response.ok) {
    throw new Error(
      `Failed to fetch WASM: ${response.status} ${response.statusText}`,
    );
  }

  const bytes = await response.arrayBuffer();
  console.log(`Loaded ${bytes.byteLength} bytes`);

  function _os_log_info(
    ptr: number,
    len: number,
    fileNamePtr: number,
    fileNameLen: number,
    lineNumber: number,
  ): void {
    const message = wasmMemory.loadString(ptr, len);
    const fileName = wasmMemory.loadString(fileNamePtr, fileNameLen);
    console.log(`${fileName}:${lineNumber}: ${message}`);
  }

  function _os_log_warn(
    ptr: number,
    len: number,
    fileNamePtr: number,
    fileNameLen: number,
    lineNumber: number,
  ): void {
    const message = wasmMemory.loadString(ptr, len);
    const fileName = wasmMemory.loadString(fileNamePtr, fileNameLen);
    console.warn(`${fileName}:${lineNumber}: ${message}`);
  }

  function _os_log_error(
    ptr: number,
    len: number,
    fileNamePtr: number,
    fileNameLen: number,
    lineNumber: number,
  ): void {
    const message = wasmMemory.loadString(ptr, len);
    const fileName = wasmMemory.loadString(fileNamePtr, fileNameLen);
    console.error(`${fileName}:${lineNumber}: ${message}`);
  }

  function _os_lock_mouse(lock: number): void {
    if (lock) {
      canvas.requestPointerLock();
    } else {
      document.exitPointerLock();
    }
  }

  function _os_is_mouse_locked(): number {
    return document.pointerLockElement === canvas ? 1 : 0;
  }

  const wasmModule = await WebAssembly.instantiate(bytes, {
    env: {
      memory,
      _os_log_info,
      _os_log_warn,
      _os_log_error,
      _os_lock_mouse,
      _os_is_mouse_locked,
      ...fsFunctions,
      ...renderer,
      _platform_audio_write_samples:
        audioFunctions._platform_audio_write_samples,
      _platform_audio_get_sample_rate:
        audioFunctions._platform_audio_get_sample_rate,
      _platform_audio_get_samples_needed:
        audioFunctions._platform_audio_get_samples_needed,
      _platform_audio_update: audioFunctions._platform_audio_update,
      _platform_audio_shutdown: audioFunctions._platform_audio_shutdown,
    },
    wasi_snapshot_preview1: {
      fd_close: (_fd: number): number => 0,
      fd_seek: (_fd: number, _offset: bigint, _whence: number, _newoffset: number): number => 0,
      fd_write: (_fd: number, _iovs: number, _iovsLen: number, _nwritten: number): number => 0,
    },
  });

  return wasmModule.instance.exports as WasmExports;
}

async function runGame(): Promise<void> {
  try {
    console.log("Loading WASM...");

    const pageCount = 16384 * 4;
    const memory = new WebAssembly.Memory({
      initial: pageCount,
      maximum: pageCount,
    });
    const wasmMemory = new WasmMemoryInterface(memory);

    const platform = {
      audioContext: null as AudioContext | null,
      audioWorkletNode: null as AudioWorkletNode | null,
    };

    const renderer = createWebGLRenderer(wasmMemory, canvas);
    const fsFunctions = createFileSystemFunctions(wasmMemory);
    const audioFunctions = createAudioFunctions(wasmMemory, platform);

    const wasmExports = await loadWasmModule(
      memory,
      wasmMemory,
      renderer,
      fsFunctions,
      audioFunctions,
    );
    console.log("WASM instantiated");

    const {
      app_init,
      app_update_and_render,
      app_on_reload,
      app_memory_size,
      os_get_heap_base,
      app_ctx_set,
      app_ctx_current,
    } = wasmExports;

    if (
      !app_init ||
      !app_update_and_render ||
      !app_on_reload ||
      !app_memory_size ||
      !os_get_heap_base ||
      !app_ctx_current ||
      !app_ctx_set
    ) {
      throw new Error("Missing required WASM exports");
    }

    const appMemoryStructSize = app_memory_size();
    console.log(`App memory struct size: ${appMemoryStructSize}`);

    const AppMemoryOffset = os_get_heap_base();
    console.log(`App memory base: ${AppMemoryOffset}`);

    const totalMemory = memory.buffer.byteLength;
    const usableMemory = totalMemory - appMemoryStructSize;
    const temporaryMemorySize = Math.floor(usableMemory / 8);
    const permanentMemorySize = usableMemory - temporaryMemorySize;
    const permanentMemoryPtr = AppMemoryOffset + appMemoryStructSize;
    const temporaryMemoryPtr = permanentMemoryPtr + permanentMemorySize;

    console.log(
      `Total memory: ${totalMemory} bytes (${totalMemory / 1024 / 1024} MB)`,
    );
    console.log(`Usable memory: ${usableMemory} bytes`);
    console.log(
      `Permanent memory: ${permanentMemorySize} bytes at offset ${permanentMemoryPtr}`,
    );
    console.log(
      `Temporary memory: ${temporaryMemorySize} bytes at offset ${temporaryMemoryPtr}`,
    );

    const appMemory = new AppMemoryWriter(
      wasmMemory,
      AppMemoryOffset,
      permanentMemorySize,
      temporaryMemorySize,
      permanentMemoryPtr,
      temporaryMemoryPtr,
    );

    const maxInputEvents = 20;

    function preventContextMenu(e: MouseEvent): boolean {
      e.preventDefault();
      e.stopPropagation();
      return false;
    }

    function handleKeyDown(event: KeyboardEvent): void {
      const button = keyCodeMap.get(event.code);
      if (
        button !== undefined &&
        appMemory.inputEvents.events.length < maxInputEvents
      ) {
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_KEYDOWN,
          keyType: button,
        });
      }
    }

    function handleKeyUp(event: KeyboardEvent): void {
      const button = keyCodeMap.get(event.code);
      if (
        button !== undefined &&
        appMemory.inputEvents.events.length < maxInputEvents
      ) {
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_KEYUP,
          keyType: button,
        });
      }
    }

    function handleMouseMove(event: MouseEvent): void {
      if (document.pointerLockElement === canvas) {
        appMemory.inputEvents.mouseX += event.movementX;
        appMemory.inputEvents.mouseY += event.movementY;
      } else {
        const rect = canvas.getBoundingClientRect();
        appMemory.inputEvents.mouseX = event.clientX - rect.left;
        appMemory.inputEvents.mouseY = event.clientY - rect.top;
      }
    }

    function handleMouseDown(event: MouseEvent): void {
      const button = mouseButtonMap[event.button];
      if (
        button !== undefined &&
        appMemory.inputEvents.events.length < maxInputEvents
      ) {
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_KEYDOWN,
          keyType: button,
        });
      }
    }

    function handleMouseUp(event: MouseEvent): void {
      const button = mouseButtonMap[event.button];
      if (
        button !== undefined &&
        appMemory.inputEvents.events.length < maxInputEvents
      ) {
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_KEYUP,
          keyType: button,
        });
      }
    }

    function handleTouchStart(event: TouchEvent): void {
      event.preventDefault();
      for (
        let i = 0;
        i < event.changedTouches.length &&
        appMemory.inputEvents.events.length < maxInputEvents;
        i++
      ) {
        const touch = event.changedTouches[i];
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_TOUCH_START,
          touchId: touch.identifier,
          touchX: touch.clientX,
          touchY: touch.clientY,
        });
      }
    }

    function handleTouchEnd(event: TouchEvent): void {
      event.preventDefault();
      for (
        let i = 0;
        i < event.changedTouches.length &&
        appMemory.inputEvents.events.length < maxInputEvents;
        i++
      ) {
        const touch = event.changedTouches[i];
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_TOUCH_END,
          touchId: touch.identifier,
          touchX: touch.clientX,
          touchY: touch.clientY,
        });
      }
    }

    function handleTouchMove(event: TouchEvent): void {
      event.preventDefault();
      for (
        let i = 0;
        i < event.changedTouches.length &&
        appMemory.inputEvents.events.length < maxInputEvents;
        i++
      ) {
        const touch = event.changedTouches[i];
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_TOUCH_MOVE,
          touchId: touch.identifier,
          touchX: touch.clientX,
          touchY: touch.clientY,
        });
      }
    }

    function handleWheel(event: WheelEvent): void {
      if (appMemory.inputEvents.events.length < maxInputEvents) {
        appMemory.inputEvents.events.push({
          type: InputEventType.INPUT_EVENT_SCROLL,
          scrollDeltaX: event.deltaX,
          scrollDeltaY: event.deltaY,
        });
      }
    }

    function handleResize(): void {
      canvas.width = window.innerWidth * window.devicePixelRatio;
      canvas.height = window.innerHeight * window.devicePixelRatio;
    }

    function handleFocus(): void {
      canvas.requestPointerLock();
    }

    function handleBlur(): void {
      document.exitPointerLock();
    }

    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    window.addEventListener("resize", handleResize);
    canvas.addEventListener("mousedown", handleMouseDown);
    canvas.addEventListener("mouseup", handleMouseUp);
    canvas.addEventListener("mousemove", handleMouseMove);
    canvas.addEventListener("contextmenu", preventContextMenu);
    canvas.addEventListener("touchstart", handleTouchStart, { passive: false });
    canvas.addEventListener("touchend", handleTouchEnd, { passive: false });
    canvas.addEventListener("touchmove", handleTouchMove, { passive: false });
    canvas.addEventListener("wheel", handleWheel);
    canvas.addEventListener("focus", handleFocus);
    canvas.addEventListener("blur", handleBlur);
    handleResize();

    appMemory.time.now = performance.now() / 1000;
    appMemory.time.dt = 0;
    appMemory.canvas.width = canvas.width / window.devicePixelRatio;
    appMemory.canvas.height = canvas.height / window.devicePixelRatio;
    appMemory.write();

    console.log("Initializing audio...");
    await audioFunctions.initializeAudio();
    console.log("Audio initialized");

    console.log("Initializing game...");
    app_init(AppMemoryOffset);
    console.log("Game initialized");

    const targetFPS = 60;
    const targetFrameTimeMs = 1000 / targetFPS;
    const sleepToleranceMs = 3;
    const loopSleepToleranceMs = 1.5;

    const time = {
      nowMs: 0,
      lastMs: 0,
      dt: 0,
    };

    let isRunning = true;

    const gameState: GameState = {
      memory,
      wasmMemory,
      appMemory,
      appMemoryOffset: AppMemoryOffset,
      exports: {
        app_init,
        app_update_and_render,
        app_on_reload,
        app_ctx_set,
        app_ctx_current,
      },
      renderer,
      fsFunctions,
      audioFunctions,
      platform,
      isRunning: true,
    };

    function tick(): void {
      if (!isRunning || !gameState.isRunning) {
        return;
      }

      {
        // let startFrameMs = performance.now();
        // let lastStartFrameMs = time.nowMs;
        // let lastFrameDtMs = startFrameMs - lastStartFrameMs;
        //
        // if (lastFrameDtMs < targetFrameTimeMs - sleepToleranceMs) {
        //   requestAnimationFrame(tick);
        //   return;
        // }
        //
        // while (lastFrameDtMs < targetFrameTimeMs - loopSleepToleranceMs) {
        //   startFrameMs = performance.now();
        //   lastFrameDtMs = startFrameMs - lastStartFrameMs;
        // }
      }

      time.nowMs = performance.now();
      time.dt = time.nowMs - time.lastMs;
      time.lastMs = time.nowMs;

      appMemory.time.now = time.nowMs / 1000;
      appMemory.time.dt = time.dt / 1000;

      appMemory.canvas.width = canvas.width / window.devicePixelRatio;
      appMemory.canvas.height = canvas.height / window.devicePixelRatio;

      appMemory.write();

      gameState.exports.app_update_and_render(AppMemoryOffset);

      requestAnimationFrame(tick);
    }

    async function hotReload(): Promise<void> {
      console.log("Hot reloading WASM...");
      gameState.isRunning = false;

      try {
        const current_app_ctx_ptr = gameState.exports.app_ctx_current();
        const newExports = await loadWasmModule(
          gameState.memory,
          gameState.wasmMemory,
          gameState.renderer,
          gameState.fsFunctions,
          gameState.audioFunctions,
        );

        gameState.exports.app_init = newExports.app_init;
        gameState.exports.app_update_and_render =
          newExports.app_update_and_render;
        gameState.exports.app_on_reload = newExports.app_on_reload;
        gameState.exports.app_ctx_current = newExports.app_ctx_current;
        gameState.exports.app_ctx_set = newExports.app_ctx_set;

        gameState.exports.app_ctx_set(current_app_ctx_ptr)

        console.log("Calling app_on_reload...");
        gameState.exports.app_on_reload(gameState.appMemoryOffset);
        console.log("Hot reload complete");

        gameState.isRunning = true;
        requestAnimationFrame(tick);
      } catch (err) {
        console.error("Hot reload failed, keeping old module:", err);
        gameState.isRunning = true;
        requestAnimationFrame(tick);
      }
    }

    // if (process.env.NODE_ENV === 'development') {
    const ws = new WebSocket(`ws://${window.location.host}/hot-reload`);
    ws.onmessage = (event) => {
      if (event.data === "reload") {
        hotReload();
      }
    };
    ws.onerror = (err) => {
      console.log(
        "Hot reload WebSocket not available (not running dev server?)",
      );
    };
    // }

    console.log("Starting game loop at 60 FPS");
    requestAnimationFrame(tick);

    (window as any).wasmExports = wasmExports;
    (window as any).hotReload = hotReload;
  } catch (err) {
    console.error("Failed to load WASM:", err);
    throw err;
  }
}

runGame();
