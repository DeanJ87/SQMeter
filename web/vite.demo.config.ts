import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

export default defineConfig({
  plugins: [preact()],
  base: "/SQMeter/demo/",
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
