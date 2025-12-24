import { readString } from "./shared";

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

export function createFileSystemFunctions(memory: WebAssembly.Memory) {
  let nextFileReadId = 1;
  const pendingFileReads = new Map<number, FileReadOp>();
  const completedFileReads = new Map<number, FileReadResult>();

  const FileReadOpState = {
    NONE: 0,
    IN_PROGRESS: 1,
    COMPLETED: 2,
    ERROR: 3,
  };

  const FileReadResultCode = {
    SUCCESS: 0,
    FAIL: 1,
  };

  function toAbsolutePath(path: string): string {
    return path.startsWith("/") ? path : "/" + path;
  }

  function _os_start_read_file(
    filenamePtr: number,
    filenameLen: number,
  ): number {
    const fileName = readString(memory, filenamePtr, filenameLen);

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

  function _os_get_file_data(
    opId: number,
    bufferPtr: number,
    bufferLen: number,
  ): number {
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

    new Uint8Array(memory.buffer, bufferPtr, result.data.length).set(result.data);

    completedFileReads.delete(opId);

    return 0;
  }

  return {
    _os_start_read_file,
    _os_check_read_file,
    _os_get_file_size,
    _os_get_file_data,
  };
}
