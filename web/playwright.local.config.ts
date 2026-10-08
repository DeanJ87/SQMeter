import base from './playwright.config';
import { defineConfig } from '@playwright/test';

// Local only (not committed): a port of its own for parallel runs.
const PORT = 4183;
export default defineConfig({
  ...base,
  use: { ...base.use, baseURL: `http://localhost:${PORT}/` },
  webServer: {
    command: `npx vite preview --config vite.demo.config.ts --port ${PORT} --strictPort`,
    url: `http://localhost:${PORT}/`,
    reuseExistingServer: false,
    timeout: 30_000,
  },
});
