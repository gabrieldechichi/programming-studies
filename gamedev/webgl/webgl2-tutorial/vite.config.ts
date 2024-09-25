import path from "node:path";
import FullReload from "vite-plugin-full-reload";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [FullReload(["./assets/*.wasm", "./assets/shaders/*"])],
  resolve: {
    alias: {
      src: path.resolve(__dirname, "./src"),
      assets: path.resolve(__dirname, "./assets"),
    },
  },
  server: {
    watch: {
      ignored: ["!web/assets/**/*.wasm"],
      usePolling: true,
    },
  },
});
