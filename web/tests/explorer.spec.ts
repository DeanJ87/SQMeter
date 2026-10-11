import { test, expect, type Page } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';
import openapi from '../../docs/api/openapi.json' with { type: 'json' };

// The demo site's API explorer (specs/028-openapi FR-010): the OpenAPI
// description in Scalar, "Try it" answered by the emulated device in the
// browser, themed like the docs site, and nothing leaves the browser.

const outsideRequests = (page: Page) => {
  const outside: string[] = [];
  page.on('request', (request) => {
    const url = new URL(request.url());
    if (url.protocol.startsWith('http') && url.origin !== new URL(page.url() || request.url()).origin && url.hostname !== 'localhost') {
      outside.push(request.url());
    }
  });
  return outside;
};

const open = async (page: Page) => {
  await page.goto('./explorer/');
  await expect(page.getByRole('heading', { name: 'SQMeter REST API' })).toBeVisible({ timeout: 15_000 });
};

test.describe('API explorer', () => {
  test('lists every operation and is themed like the docs site', async ({ page }) => {
    const outside = outsideRequests(page);
    await open(page);
    for (const tag of ['Readings', 'Safety', 'Settings', 'Alerts', 'Updates']) {
      await expect(page.getByRole('link', { name: tag, exact: true }).first()).toBeVisible();
    }
    const body = await page.evaluate(() => getComputedStyle(document.body).backgroundColor);
    expect(body).toBe('rgb(8, 13, 20)'); // --sqm-bg
    const font = await page.getByRole('heading', { name: 'SQMeter REST API' }).evaluate((el) => getComputedStyle(el).fontFamily);
    expect(font).toContain('Inter SQM');
    expect(outside).toEqual([]);
  });

  test('"Try it" is answered by the emulated device', async ({ page }) => {
    const outside = outsideRequests(page);
    await open(page);
    await page
      .getByRole('button', { name: /Test Request.*get \/api\/sensors/i })
      .first()
      .click();
    const send = page.getByRole('button', { name: /^Send get request to .*\/api\/sensors$/ });
    await expect(send).toBeVisible();
    await send.click();
    await expect(page.getByText(/"timeValid"/).first()).toBeVisible({ timeout: 10_000 });
    expect(outside).toEqual([]);
  });

  test('every documented GET route answers in the explorer page with JSON', async ({ page }) => {
    const outside = outsideRequests(page);
    await open(page);
    const gets = Object.entries(openapi.paths)
      .filter(([, ops]) => 'get' in ops)
      .map(([path]) => path);
    expect(gets.length).toBeGreaterThan(10);
    const answers = await page.evaluate(async (paths) => {
      const out: Record<string, number> = {};
      for (const path of paths) out[path] = (await fetch(path)).status;
      return out;
    }, gets);
    for (const path of gets) expect([200, 202], path).toContain(answers[path]);
    expect(outside).toEqual([]);
  });

  test('passes the accessibility checks as loaded', async ({ page }) => {
    await open(page);
    await page.waitForTimeout(500);
    const results = await new AxeBuilder({ page }).withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa']).analyze();
    expect(results.violations.map((v) => `${v.id}: ${v.nodes.map((n) => n.target).join(', ')}`)).toEqual([]);
  });

  test('works on a phone', async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    await open(page);
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
    expect(overflow).toBeLessThanOrEqual(1);
  });
});
