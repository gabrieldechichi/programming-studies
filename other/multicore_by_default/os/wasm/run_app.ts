import { watch } from "fs";
import { resolve } from "path";

const buildMode = process.argv[2] || "release";
if (buildMode !== "debug" && buildMode !== "release") {
  console.error(`Invalid build mode: ${buildMode}. Use 'debug' or 'release'`);
  process.exit(1);
}

const PORT = 8000;
const STATIC_DIR = resolve(import.meta.dir, `../../../web/dist/${buildMode}`);
const WASM_PATH = resolve(STATIC_DIR, "game.wasm");

process.chdir(STATIC_DIR);
console.log(`Working directory: ${process.cwd()}`);

const clients = new Set<ServerWebSocket<unknown>>();

const server = Bun.serve({
  port: PORT,
  async fetch(req, server) {
    const url = new URL(req.url);

    if (url.pathname === "/hot-reload") {
      const success = server.upgrade(req);
      if (!success) {
        return new Response("WebSocket upgrade failed", { status: 400 });
      }
      return undefined;
    }

    let filePath = url.pathname === "/" ? "/index.html" : decodeURIComponent(url.pathname);
    const fullPath = resolve(STATIC_DIR + filePath);

    if (!fullPath.startsWith(STATIC_DIR)) {
      return new Response("Forbidden", { status: 403 });
    }

    const file = Bun.file(fullPath);
    const exists = await file.exists();

    if (!exists) {
      return new Response("Not Found", { status: 404 });
    }

    return new Response(file, {
      headers: {
        'Cache-Control': 'no-store, no-cache, must-revalidate',
        'Pragma': 'no-cache',
        'Expires': '0'
      }
    });
  },

  websocket: {
    open(ws) {
      clients.add(ws);
      console.log("Client connected. Total clients:", clients.size);
    },
    close(ws) {
      clients.delete(ws);
      console.log("Client disconnected. Total clients:", clients.size);
    },
    message() {},
  },
});

console.log(`Dev server running at http://localhost:${PORT}`);
console.log(`Watching for changes: ${WASM_PATH}`);

watch(WASM_PATH, (event) => {
  if (event === "change") {
    console.log("WASM file changed, triggering hot reload...");
    for (const client of clients) {
      client.send("reload");
    }
  }
});

const openBrowser = async () => {
  const url = `http://localhost:${PORT}`;
  const platform = process.platform;

  if (platform === "darwin") {
    await Bun.spawn(["open", url]);
  } else if (platform === "win32") {
    await Bun.spawn(["cmd", "/c", "start", url]);
  } else {
    await Bun.spawn(["xdg-open", url]);
  }

  console.log(`Opened browser at ${url}`);
};

openBrowser();
