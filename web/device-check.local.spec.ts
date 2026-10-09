import { test, expect } from '@playwright/test';

// One-off check against the spare device (not committed).
const SHOTS = '/private/tmp/claude-501/-Users-dean0-Projects-SQMeter/f1cfc810-0633-4782-a38e-69e0ad66a869/scratchpad/lang-shots';

test('device: language switch feedback', async ({ page }) => {
  test.setTimeout(120_000);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto('http://192.168.1.128/settings?tab=device');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2000);
  await page.screenshot({ path: `${SHOTS}/0-notice-after-reload.png`, fullPage: false });
  const notice = await page.locator('main .note').first().innerText();
  console.log('NOTICE:', notice);

  await page.locator('[data-field="language"]').selectOption('es');
  await page.getByRole('button', { name: 'Save' }).click();
  const banner = page.locator('.language-progress');
  await expect(banner).toBeVisible({ timeout: 10_000 });
  console.log('PHASE1:', await banner.getAttribute('data-phase'), '|', await banner.innerText());
  await page.screenshot({ path: `${SHOTS}/1-downloading.png` });
  await expect(banner).toHaveAttribute('data-phase', /failed|installed/, { timeout: 60_000 });
  console.log('PHASE2:', await banner.getAttribute('data-phase'), '|', await banner.innerText());
  await page.screenshot({ path: `${SHOTS}/2-result.png` });
  console.log('URL:', page.url());

  // Back to English.
  await page.locator('[data-field="language"]').selectOption('en');
  await page.getByRole('button', { name: 'Save' }).click();
  await page.waitForTimeout(4000);
  console.log('FINAL URL:', page.url());
});
