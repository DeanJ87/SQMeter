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

test.beforeEach(async ({ page }) => {
  // Give MSW service worker time to activate before each test
  await page.addInitScript(() => {
    window.__mswReady = new Promise<void>((resolve) => {
      navigator.serviceWorker.ready.then(() => resolve());
    });
  });
});

test("dashboard", async ({ page }) => {
  await page.goto("/");
  await page.waitForTimeout(2000); // wait for WebSocket data to populate
  await expect(page).toHaveScreenshot({ path: save("dashboard") });
});

test("settings", async ({ page }) => {
  await page.goto("/settings");
  await page.waitForLoadState("networkidle");
  await expect(page).toHaveScreenshot({ path: save("settings") });
});

test("system", async ({ page }) => {
  await page.goto("/system");
  await page.waitForLoadState("networkidle");
  await expect(page).toHaveScreenshot({ path: save("system") });
});

test("updates", async ({ page }) => {
  await page.goto("/updates");
  await page.waitForLoadState("networkidle");
  await expect(page).toHaveScreenshot({ path: save("updates") });
});
