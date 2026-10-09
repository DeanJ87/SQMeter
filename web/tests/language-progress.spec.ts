import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { expect, test, type Page } from '@playwright/test';

// Switching language shows what the device is doing (specs/023-i18n FR-024):
// downloading, installed (then the page reloads into it), or the failure with
// what to do - and the page never leaves its own address.

const EN = JSON.parse(readFileSync(resolve(import.meta.dirname, '../src/i18n/en.json'), 'utf8')) as Record<string, string>;
const fill = (key: string, values: Record<string, string>) =>
  Object.entries(values).reduce((text, [name, value]) => text.replace(`{${name}}`, value), EN[key]);

const choose = async (page: Page, code: string) => {
  await page.goto('./#/settings?tab=device');
  await page.locator('[data-field="language"]').selectOption(code);
  await page.getByRole('button', { name: EN['settings.save'] }).click();
};

test('language: progress, then the page reloads into the language', async ({ page }) => {
  test.setTimeout(60_000);
  await choose(page, 'es');
  const origin = new URL(page.url()).origin;
  const banner = page.locator('.language-progress');
  await expect(banner).toContainText(fill('language.progressDownloading', { language: 'Español' }).split('...')[0]);
  await expect(page.locator('html')).toHaveAttribute('lang', 'es', { timeout: 20_000 });
  // The reload stays on the same address (never the setup hotspot's).
  expect(new URL(page.url()).origin).toBe(origin);
});

test('language: a failed download says why and what to do, with Retry', async ({ page }) => {
  test.setTimeout(60_000);
  await page.addInitScript(() => sessionStorage.setItem('sqm.demo.languageFail', '1'));
  await page.setViewportSize({ width: 390, height: 844 });
  await choose(page, 'de');
  const banner = page.locator('.language-progress');
  await expect(banner).toHaveAttribute('data-phase', 'failed', { timeout: 20_000 });
  // On a phone, Save is at the bottom: the outcome must be on screen there.
  await expect(banner).toBeInViewport();
  await expect(banner).toContainText("Couldn't download the language file for this firmware version");
  await expect(banner).toContainText(fill('language.progressFailedHelp', { version: '0.2.0-beta.3' }));
  await expect(page.locator('html')).toHaveAttribute('lang', 'en');

  // Retry runs the download again and lands on the same answer.
  await banner.getByRole('button', { name: EN['language.retry'] }).click();
  await expect(banner).toHaveAttribute('data-phase', 'downloading');
  await expect(banner).toHaveAttribute('data-phase', 'failed', { timeout: 20_000 });

  // After a reload, every page says why the UI is still in English.
  await page.reload();
  await expect(page.locator('main')).toContainText(
    fill('layout.languageUnavailableReason', { reason: "Couldn't download the language file for this firmware version" }),
  );
});
