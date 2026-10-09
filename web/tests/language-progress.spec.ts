import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { expect, test, type Page } from '@playwright/test';

// Switching language reports inside the Language card (specs/023 FR-024,
// 026 FR-004, DS-06): the language with a pill and a progress meter, then the
// outcome on one line - never in the header or on other pages - and the page
// never leaves its own address.

const EN = JSON.parse(readFileSync(resolve(import.meta.dirname, '../src/i18n/en.json'), 'utf8')) as Record<string, string>;

const choose = async (page: Page, code: string) => {
  await page.goto('./#/settings?tab=device');
  await page.locator('[data-field="language"]').selectOption(code);
  await page.getByRole('button', { name: EN['settings.save'] }).click();
};

const steps = (page: Page) => page.locator('#language .language-steps');

test('language: progress in the card, then the page reloads into the language', async ({ page }) => {
  test.setTimeout(60_000);
  await choose(page, 'es');
  const origin = new URL(page.url()).origin;
  await expect(steps(page)).toContainText('Español');
  await expect(steps(page).locator('.pill')).toHaveText(EN['status.downloading']);
  await expect(steps(page).getByRole('progressbar')).toBeVisible();
  // Nothing in the header (DS-06).
  await expect(page.locator('.app-header [role="status"]')).toHaveCount(0);
  await expect(page.locator('html')).toHaveAttribute('lang', 'es', { timeout: 20_000 });
  // The reload stays on the same address (never the setup hotspot's).
  expect(new URL(page.url()).origin).toBe(origin);
});

test('language: a failed download says why on one line, with Retry', async ({ page }) => {
  test.setTimeout(60_000);
  await page.addInitScript(() => sessionStorage.setItem('sqm.demo.languageFail', '1'));
  await page.setViewportSize({ width: 390, height: 844 });
  await choose(page, 'de');
  await expect(steps(page)).toHaveAttribute('data-phase', 'failed', { timeout: 20_000 });
  // On a phone, Save is at the bottom: the card scrolls the outcome into view.
  await expect(steps(page)).toBeInViewport();
  await expect(steps(page).locator('.pill')).toHaveText(EN['language.phaseFailed']);
  await expect(steps(page)).toContainText("Couldn't download the language file for this firmware version");
  await expect(page.locator('html')).toHaveAttribute('lang', 'en');
  await expect(page.locator('.app-header [role="status"]')).toHaveCount(0);

  // Retry runs the download again and lands on the same answer.
  await steps(page).getByRole('button', { name: EN['language.retry'] }).click();
  await expect(steps(page)).toHaveAttribute('data-phase', 'downloading');
  await expect(steps(page)).toHaveAttribute('data-phase', 'failed', { timeout: 20_000 });

  // After a reload, the Language card says why (not every page).
  await page.reload();
  await page.goto('./#/settings?tab=device&section=language');
  await expect(page.locator('[data-language-reason]')).toHaveText("Couldn't download the language file for this firmware version");
  await page.goto('./#/system');
  await expect(page.locator('main')).not.toContainText("Couldn't download the language file");
});

test('language: a download a restart cut short shows in the Status card and in Settings', async ({ page }) => {
  // The Status card row is the dashboard inventory's `language` entry (web/tests/dashboard.spec.ts).
  test.setTimeout(60_000);
  const reason = EN['device.language.theLastLanguageDownloadDidnT'];
  await page.addInitScript(() => sessionStorage.setItem('sqm.demo.languageFail', 'interrupted'));
  await choose(page, 'fr');
  await expect(steps(page)).toHaveAttribute('data-phase', 'failed', { timeout: 20_000 });
  // As after the device's next boot: the page loads with the language unavailable.
  await page.goto('./#/');
  await page.reload();
  const row = page.locator('[data-inventory="language"]');
  await expect(row).toContainText(EN['language.label']);
  await expect(row).toContainText(EN['status.notLoaded']);
  await expect(row.getByRole('link')).toHaveAttribute('href', /tab=device&section=language/);
  await page.goto('./#/settings?tab=device&section=language');
  await expect(page.locator('[data-language-reason]')).toHaveText(reason);
});
