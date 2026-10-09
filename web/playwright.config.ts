import { defineConfig, devices } from '@playwright/test';

const BASE_URL = 'http://localhost:4173/';

/** Browser state with the demo tour already dismissed, so the first-visit
 * offer (specs/018) doesn't sit over the page in other tests. Tour tests
 * start from a clean state instead. */
export const tourDismissed = (baseURL: string) => ({
  cookies: [],
  origins: [{ origin: new URL(baseURL).origin, localStorage: [{ name: 'sqm.demo.tour.v1', value: 'dismissed' }] }],
});

export default defineConfig({
  testDir: './tests',
  // The docs check needs the built mkdocs site: playwright.docs.config.ts.
  testIgnore: 'a11y-docs.spec.ts',
  fullyParallel: false,
  retries: 1,
  workers: 1,
  reporter: 'list',

  use: {
    baseURL: BASE_URL,
    storageState: tourDismissed(BASE_URL),
    // Give MSW time to intercept before assertions
    actionTimeout: 10_000,
    screenshot: 'only-on-failure',
    colorScheme: 'dark',
    viewport: { width: 1280, height: 800 },
  },

  projects: [
    {
      name: 'chromium',
      use: {
        ...devices['Desktop Chrome'],
        // GitHub-hosted runners already include Chrome. Avoid downloading a
        // second browser during every Pages deployment.
        channel: process.env.CI ? 'chrome' : undefined,
      },
    },
  ],

  // Start the demo preview server before running tests
  webServer: {
    command: 'npm run preview:demo',
    url: BASE_URL,
    reuseExistingServer: !process.env.CI,
    timeout: 30_000,
  },
});
