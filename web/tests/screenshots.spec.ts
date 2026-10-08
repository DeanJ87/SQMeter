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

// The dashboard re-lays out its cards as live data arrives (with a short
// transition); wait until card positions and sizes stop changing.
const waitForLayout = async (page: import("@playwright/test").Page) => {
  await page.evaluate(async () => {
    const snapshot = () =>
      Array.from(document.querySelectorAll(".masonry-item"))
        .map((el) => {
          const r = el.getBoundingClientRect();
          return `${Math.round(r.x)},${Math.round(r.y)},${Math.round(r.height)}`;
        })
        .join("|");
    let last = "";
    let stableSince = performance.now();
    const deadline = performance.now() + 8000;
    while (performance.now() < deadline) {
      await new Promise((resolve) => setTimeout(resolve, 100));
      const now = snapshot();
      if (now !== last) {
        last = now;
        stableSince = performance.now();
      } else if (performance.now() - stableSince > 700) {
        return;
      }
    }
  });
};

const capturePage = async (
  page: import("@playwright/test").Page,
  name: string
) => {
  await waitForLayout(page);
  await page.screenshot({ path: save(name), fullPage: true });
};

// The demo uses hash routing so GitHub Pages hard-refreshes never 404.
// All goto() calls use "./" or "./#/route" — resolved against the Playwright
// baseURL (http://localhost:4173/) so they reach the demo server.

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
  await expect(page.getByRole("heading", { name: "Firmware" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Manual upload" })).toBeVisible();
  await capturePage(page, "updates");
});

test("alpaca", async ({ page }) => {
  await page.goto("./#/alpaca");
  await waitForDemoApp(page);
  await expect(page.getByRole("heading", { name: "ASCOM Alpaca" })).toBeVisible();
  await capturePage(page, "alpaca");
});

for (const [id, label] of [
  ["device", "Device"],
  ["network", "Network"],
  ["time", "Time & Location"],
  ["sensors", "Sensors"],
  ["safety", "Safety"],
  ["alerts", "Alerts"],
] as const) {
  test(`settings ${label}`, async ({ page }) => {
    await page.goto(`./#/settings?tab=${id}`);
    await waitForDemoApp(page);
    const tab = page.getByRole("tab", { name: label });
    await tab.click();
    await expect(tab).toHaveAttribute("aria-selected", "true");
    await capturePage(page, `settings-${id}`);
  });
}

test("alerts flyout", async ({ page }) => {
  await page.goto("./");
  await waitForDemoApp(page);
  await waitForLayout(page);
  await page.getByRole("button", { name: /^Alerts/ }).click();
  const flyout = page.getByRole("dialog", { name: "Recent alerts" });
  await expect(flyout).toBeVisible();
  await waitForLayout(page);
  await page.screenshot({ path: save("alerts-flyout") });
});
