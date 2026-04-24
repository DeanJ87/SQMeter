import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
  testDir: "./tests",
  fullyParallel: false,
  retries: 1,
  workers: 1,
  reporter: "list",

  use: {
    baseURL: "http://localhost:4173/SQMeter/demo/",
    // Give MSW time to intercept before assertions
    actionTimeout: 10_000,
    screenshot: "only-on-failure",
    colorScheme: "dark",
    viewport: { width: 1280, height: 800 },
  },

  projects: [
    {
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },
  ],

  // Start the demo preview server before running tests
  webServer: {
    command: "npm run preview:demo",
    url: "http://localhost:4173/SQMeter/demo/",
    reuseExistingServer: !process.env.CI,
    timeout: 30_000,
  },
});
