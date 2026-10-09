import { test, expect, type Page } from '@playwright/test';
import { readFileSync } from 'fs';
import { resolve } from 'path';

// Numbers follow the active language (specs/023-i18n FR-017): typed decimals
// use the language's separator and reach the device exactly; shown values use
// it too, with Latin digits in every language.

const LOCALES = resolve(import.meta.dirname, '../src/i18n/locales');
const messagesFor = (code: string): Record<string, string> => JSON.parse(readFileSync(resolve(LOCALES, `${code}.json`), 'utf8'));

const open = async (page: Page, code: string, route: string) => {
  await page.goto('about:blank');
  await page.goto(`./?lang=${code}${route}`);
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(1500); // first ticks of the emulated device
};

const config = (page: Page) => page.evaluate(async () => (await fetch('/api/config')).json());

test('German decimals typed into a setting reach the device', async ({ page }) => {
  await open(page, 'de', '#/settings?tab=sensors');
  const field = page.locator('[data-field="cloudDetection.clearSkyThreshold"]');
  await expect(field).toHaveValue('-13');
  await field.fill('-14,5');
  await field.press('Tab');
  await page.getByRole('button', { name: messagesFor('de')['settings.save'] }).click();
  await expect.poll(async () => (await config(page)).cloudDetection.clearSkyThreshold).toBe(-14.5);
  await expect(field).toHaveValue('-14,5');
});

test('an unclear number is kept with a message, not truncated', async ({ page }) => {
  await open(page, 'de', '#/settings?tab=sensors');
  const field = page.locator('[data-field="cloudDetection.clearSkyThreshold"]');
  await field.fill('-1.234');
  await field.press('Tab');
  await expect(field).toHaveValue('-1.234');
  await expect(field).toHaveAttribute('aria-invalid', 'true');
  expect((await config(page)).cloudDetection.clearSkyThreshold).toBe(-13);
});

for (const code of ['fr', 'ar']) {
  test(`${code}: System and Alpaca pages show numbers the language's way`, async ({ page }, testInfo) => {
    for (const route of ['#/system', '#/alpaca']) {
      await open(page, code, route);
      const text = await page.locator('main').innerText();
      expect(text, 'Latin digits only').not.toMatch(/[٠-٩۰-۹]/);
      if (code === 'fr' && route === '#/system') expect(text).toMatch(/\d+,\d{2} (KB|MB)/);
      if (code === 'ar' && route === '#/system') expect(text).toMatch(/\d+\.\d{2} (KB|MB)/);
      await testInfo.attach(`${code}${route.replace('#/', '-')}.png`, {
        body: await page.screenshot({ fullPage: true }),
        contentType: 'image/png',
      });
    }
  });
}
