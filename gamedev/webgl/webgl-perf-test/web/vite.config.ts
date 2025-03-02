import path from "path";
import FullReload from "vite-plugin-full-reload";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [FullReload(["./public/*.wasm", "./public/shaders/*"])],
  base: "./",
  resolve: {
    alias: {
      src: path.resolve(__dirname, "./src"),
      public: path.resolve(__dirname, "./public"),
    },
  },
  server: {
    watch: {
      ignored: ["!web/public/**/*.wasm"],
      usePolling: true,
    },
  },
});
