import path from "path";
import FullReload from "vite-plugin-full-reload";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [FullReload(["./resources/*.wasm", "./resources/shaders/*"])],
  resolve: {
    alias: {
      src: path.resolve(__dirname, "./src"),
      resources: path.resolve(__dirname, "./resources"),
    },
  },
  server: {
    watch: {
      ignored: ["!web/resources/**/*.wasm"],
      usePolling: true,
    },
  },
});
