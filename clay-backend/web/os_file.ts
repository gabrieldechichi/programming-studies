import type { WasmMemoryInterface } from "./wasm.ts";

interface FileReadOp {
  fileName: string;
  id: number;
}

interface FileReadResult {
  id: number;
  fileName: string;
  code: number;
  data: Uint8Array | null;
}

interface TextureHandle {
  idx: number;
  gen: number;
}

interface WebPLoad {
  filePath: string;
  handle: TextureHandle;
}

interface WebPLoadResult {
  success: boolean;
}

const FileReadOpState = {
  NONE: 0,
  IN_PROGRESS: 1,
  COMPLETED: 2,
  ERROR: 3,
} as const;

const FileReadResultCode = {
  SUCCESS: 0,
  FAIL: 1,
} as const;

export function createFileSystemFunctions(wasmMemory: WasmMemoryInterface) {
  let nextFileReadId = 1;
  const pendingFileReads = new Map<number, FileReadOp>();
  const completedFileReads = new Map<number, FileReadResult>();

  let nextWebPLoadId = 1;
  const pendingWebPLoads = new Map<number, WebPLoad>();
  const completedWebPLoads = new Map<number, WebPLoadResult>();

  function toAbsolutePath(path: string): string {
    return path.startsWith("/") ? path : "/" + path;
  }

  function _os_start_read_file(filenamePtr: number, filenameLen: number): number {
    const fileName = wasmMemory.loadString(filenamePtr, filenameLen);

    const id = nextFileReadId;
    nextFileReadId++;

    const op: FileReadOp = { fileName, id };
    const result: FileReadResult = {
      id,
      fileName,
      code: FileReadResultCode.FAIL,
      data: null,
    };

    pendingFileReads.set(op.id, op);

    fetch(toAbsolutePath(fileName))
      .then((response) => {
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const contentType = response.headers.get("content-type") || "";
        if (contentType.includes("text/html") && !fileName.endsWith(".html")) {
          throw new Error(`File not found: ${fileName} (got HTML fallback)`);
        }

        return response.arrayBuffer();
      })
      .then((data) => {
        result.data = new Uint8Array(data);
        result.code = FileReadResultCode.SUCCESS;
        completedFileReads.set(op.id, result);
        pendingFileReads.delete(op.id);
      })
      .catch((error) => {
        console.error(`Error reading file ${fileName}: ${error.message}`);
        result.code = FileReadResultCode.FAIL;
        completedFileReads.set(op.id, result);
        pendingFileReads.delete(op.id);
      });

    return id;
  }

  function _os_check_read_file(opId: number): number {
    if (pendingFileReads.has(opId)) {
      return FileReadOpState.IN_PROGRESS;
    }

    if (completedFileReads.has(opId)) {
      const result = completedFileReads.get(opId);
      if (result?.code === FileReadResultCode.FAIL) {
        return FileReadOpState.ERROR;
      }
      return FileReadOpState.COMPLETED;
    }

    return FileReadOpState.NONE;
  }

  function _os_get_file_size(opId: number): number {
    const state = _os_check_read_file(opId);
    if (state !== FileReadOpState.COMPLETED) {
      return -1;
    }
    const result = completedFileReads.get(opId);
    if (!result?.data) {
      return -1;
    }

    return result.data.byteLength;
  }

  function _os_get_file_data(opId: number, bufferPtr: number, bufferLen: number): number {
    const state = _os_check_read_file(opId);
    if (state !== FileReadOpState.COMPLETED) {
      return -1;
    }

    const result = completedFileReads.get(opId);
    if (!result?.data) {
      return -1;
    }

    if (result.data.length > bufferLen) {
      return -1;
    }

    new Uint8Array(wasmMemory.memory.buffer, bufferPtr).set(result.data);

    completedFileReads.delete(opId);

    return 0;
  }

  function _os_start_webp_texture_load(
    filePathPtr: number,
    filePathLen: number,
    handleIdx: number,
    handleGen: number,
  ): number {
    const filePath = wasmMemory.loadString(filePathPtr, filePathLen);
    const handle: TextureHandle = { idx: handleIdx, gen: handleGen };
    const loadId = nextWebPLoadId++;

    console.log(
      `Starting WebP texture load: ${filePath}, handle:`,
      handle,
      "loadId:",
      loadId,
    );

    pendingWebPLoads.set(loadId, { filePath, handle });

    loadWebPTexture(filePath, handle, loadId);

    return loadId;
  }

  function _os_check_webp_texture_load(loadId: number): number {
    if (pendingWebPLoads.has(loadId)) {
      return FileReadOpState.IN_PROGRESS;
    }

    if (completedWebPLoads.has(loadId)) {
      const result = completedWebPLoads.get(loadId);
      return result?.success
        ? FileReadOpState.COMPLETED
        : FileReadOpState.ERROR;
    }

    return FileReadOpState.NONE;
  }

  return {
    _os_start_read_file,
    _os_check_read_file,
    _os_get_file_size,
    _os_get_file_data,
    _os_start_webp_texture_load,
    _os_check_webp_texture_load,
  };
}

// This function is referenced but not defined in this module
// It's likely defined in renderer.js or another module
declare function loadWebPTexture(
  filePath: string,
  handle: TextureHandle,
  loadId: number,
): void;
