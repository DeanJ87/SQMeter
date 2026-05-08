import { test, expect } from "@playwright/test";
import { fileURLToPath } from "url";
import path from "path";
import fs from "fs";

// ESM-compatible __dirname
const __dirname = path.dirname(fileURLToPath(import.meta.url));

const SCREENSHOTS_DIR = path.resolve(
  __dirname,
  "../../docs/assets/screenshots"
);

fs.mkdirSync(SCREENSHOTS_DIR, { recursive: true });

const save = (name: string) => path.join(SCREENSHOTS_DIR, `${name}.png`);

const waitForDemoApp = async (page: import("@playwright/test").Page) => {
  await page.waitForLoadState("domcontentloaded");
  await page.evaluate(() => window.__mswReady);
};

const capturePage = async (
  page: import("@playwright/test").Page,
  name: string
) => {
  await page.screenshot({ path: save(name), fullPage: true });
};

// The demo uses hash routing so GitHub Pages hard-refreshes never 404.
// All goto() calls use "./" or "./#/route" — resolved against the Playwright
// baseURL (http://localhost:4173/SQMeter/demo/) so they reach the demo server.

test.beforeEach(async ({ page }) => {
  // Give MSW service worker time to activate before each test
  await page.addInitScript(() => {
    window.__mswReady = new Promise<void>((resolve) => {
      navigator.serviceWorker.ready.then(() => resolve());
    });
  });
});

test("dashboard", async ({ page }) => {
  await page.goto("./");
  await waitForDemoApp(page);
  await expect(page.getByRole("heading", { name: "Sky Quality" })).toBeVisible();
  await expect(page.getByText("Live")).toBeVisible();
  await capturePage(page, "dashboard");
});

test("settings", async ({ page }) => {
  await page.goto("./#/settings");
  await waitForDemoApp(page);
  await expect(page.getByRole("heading", { name: "Settings" })).toBeVisible();
  await expect(page.getByText("Device Name")).toBeVisible();
  await capturePage(page, "settings");
});

test("system", async ({ page }) => {
  await page.goto("./#/system");
  await waitForDemoApp(page);
  await expect(page.getByRole("heading", { name: "Firmware" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Sensors" })).toBeVisible();
  await capturePage(page, "system");
});

test("updates", async ({ page }) => {
  await page.goto("./#/updates");
  await waitForDemoApp(page);
  await expect(page.getByRole("heading", { name: "OTA Updates" })).toBeVisible();
  await expect(page.getByText("Update Type")).toBeVisible();
  await capturePage(page, "updates");
});
