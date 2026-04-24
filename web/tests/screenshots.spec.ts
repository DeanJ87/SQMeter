import { test, expect } from "@playwright/test";
import path from "path";
import fs from "fs";

const SCREENSHOTS_DIR = path.resolve(
  __dirname,
  "../../docs/assets/screenshots"
);

// Ensure output directory exists
fs.mkdirSync(SCREENSHOTS_DIR, { recursive: true });

const save = (name: string) => path.join(SCREENSHOTS_DIR, `${name}.png`);

test.beforeEach(async ({ page }) => {
  // Wait for MSW service worker to be active before navigating
  await page.addInitScript(() => {
    window.__mswReady = new Promise<void>((resolve) => {
      navigator.serviceWorker.ready.then(() => resolve());
    });
  });
});

test("dashboard", async ({ page }) => {
  await page.goto("/");
  // Wait for live sensor data to populate (MSW WebSocket sends after ~1s)
  await page.waitForTimeout(2000);
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
