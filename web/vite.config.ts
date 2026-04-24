import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

// Set ESP32 IP address for development
// Update this to match your ESP32's IP when connected to WiFi
// Default is captive portal IP (192.168.4.1)
const ESP32_IP = "192.168.1.128";

export default defineConfig({
  plugins: [preact()],
  build: {
    outDir: "dist",
    assetsDir: "assets",
    minify: "terser",
    terserOptions: {
      compress: {
        drop_console: true,
      },
    },
    rollupOptions: {
      output: {
        manualChunks: undefined,
      },
    },
  },
  server: {
    proxy: {
      "/api": {
        target: `http://${ESP32_IP}`,
        changeOrigin: true,
      },
      "/ws": {
        target: `ws://${ESP32_IP}`,
        ws: true,
        changeOrigin: true,
      },
    },
  },
});
