export function createFileSystemFunctions(wasmMemory, renderer) {
  let nextFileReadId = 1;
  const pendingFileReads = new Map();
  const completedFileReads = new Map();

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

  let nextWebPLoadId = 1;
  const pendingWebPLoads = new Map();
  const completedWebPLoads = new Map();

  function toAbsolutePath(path) {
    return path.startsWith('/') ? path : '/' + path;
  }

  function _os_start_read_file(filenamePtr, filenameLen) {
    const fileName = wasmMemory.loadString(filenamePtr, filenameLen);

    const id = nextFileReadId;
    nextFileReadId++;

    const op = { fileName, id };
    const result = {
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

  function _os_check_read_file(opId) {
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

  function _os_get_file_size(opId) {
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

  function _os_get_file_data(opId, bufferPtr, bufferLen) {
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
    filePathPtr,
    filePathLen,
    handleIdx,
    handleGen
  ) {
    const filePath = wasmMemory.loadString(filePathPtr, filePathLen);
    const handle = { idx: handleIdx, gen: handleGen };
    const loadId = nextWebPLoadId++;

    console.log(
      `Starting WebP texture load: ${filePath}, handle:`,
      handle,
      "loadId:",
      loadId
    );

    pendingWebPLoads.set(loadId, { filePath, handle });

    loadWebPTexture(filePath, handle, loadId);

    return loadId;
  }

  async function loadWebPTexture(filePath, handle, loadId) {
    try {
      console.log(`Loading WebP texture: ${filePath} with handle:`, handle);
      const img = new Image();
      img.crossOrigin = "anonymous";

      await new Promise((resolve, reject) => {
        img.onload = () => {
          console.log(
            `WebP image loaded successfully: ${filePath}, size: ${img.naturalWidth}x${img.naturalHeight}`
          );
          resolve();
        };
        img.onerror = () => {
          console.error(`Failed to load WebP image: ${filePath}`);
          reject(new Error("Failed to load image"));
        };
        img.src = toAbsolutePath(filePath);
      });

      await img.decode();
      console.log(`WebP decode completed for: ${filePath}`);

      let texture = renderer.textures.getAssert(handle);
      if (!texture) {
        throw new Error("Texture handle not found");
      }

      texture.width = img.naturalWidth;
      texture.height = img.naturalHeight;

      if (!texture.texture) {
        texture.texture = renderer.gl.createTexture();
      }

      renderer.gl.bindTexture(renderer.gl.TEXTURE_2D, texture.texture);
      renderer.gl.texImage2D(
        renderer.gl.TEXTURE_2D,
        0,
        renderer.gl.RGBA,
        renderer.gl.RGBA,
        renderer.gl.UNSIGNED_BYTE,
        img
      );
      renderer.gl.generateMipmap(renderer.gl.TEXTURE_2D);

      renderer.textures.set(handle, texture);

      console.log(`WebP texture upload completed successfully: ${filePath}`);
      completedWebPLoads.set(loadId, { success: true });
      pendingWebPLoads.delete(loadId);
    } catch (error) {
      console.error(`Failed to load WebP texture ${filePath}:`, error);
      completedWebPLoads.set(loadId, { success: false });
      pendingWebPLoads.delete(loadId);
    }
  }

  function _os_check_webp_texture_load(loadId) {
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
