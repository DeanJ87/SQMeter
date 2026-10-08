import { defineConfig, devices } from '@playwright/test';

// Accessibility checks of the built docs site (spec 022 FR-004). Build it
// first: `mkdocs build --strict` at the repo root writes ../site.
const PORT = Number(process.env.DOCS_PORT ?? 4190);

export default defineConfig({
  testDir: './tests',
  testMatch: 'a11y-docs.spec.ts',
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: {
    baseURL: `http://localhost:${PORT}/`,
    colorScheme: 'dark',
    viewport: { width: 1280, height: 800 },
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'], channel: process.env.CI ? 'chrome' : undefined },
    },
  ],
  webServer: {
    command: `python3 -m http.server ${PORT} --directory ../site`,
    url: `http://localhost:${PORT}/`,
    reuseExistingServer: false,
    timeout: 30_000,
  },
});
