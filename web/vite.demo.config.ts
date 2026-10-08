import { defineConfig } from "vite";
import preact from "@preact/preset-vite";
import { copyFileSync } from "fs";
import { resolve } from "path";

export default defineConfig({
  plugins: [
    preact(),
    // Static hosts serve 404.html for unknown paths: let the app answer
    // device URLs (/api/..., /management/..., /setup/...) opened directly.
    {
      name: "sqm-demo-404",
      closeBundle() {
        copyFileSync(resolve(__dirname, "dist-demo/index.html"), resolve(__dirname, "dist-demo/404.html"));
      },
    },
  ],
  // Served at the root of https://demo.sqmeter.dev. Set DEMO_BASE to build it
  // for a sub-path instead (routing is hash-based, so only assets care).
  base: process.env.DEMO_BASE ?? "/",
  define: {
    "import.meta.env.VITE_DEMO_MODE": '"true"',
  },
  build: {
    outDir: "dist-demo",
    assetsDir: "assets",
    minify: "terser",
    terserOptions: {
      compress: { drop_console: false }, // keep logs in demo for transparency
    },
    rollupOptions: {
      output: { manualChunks: undefined },
    },
  },
});
