import { test, expect } from '@playwright/test';

// Settings follows the address (026 audit A23): a link, Back/Forward or a
// pasted address can change the tab while the page stays open.

test('Settings switches tab when the address changes, and with Back', async ({ page }) => {
  await page.goto('./#/settings?tab=device');
  const selected = page.locator('[role="tab"][aria-selected="true"]');
  await expect(selected).toHaveText('Device', { timeout: 15_000 });
  await page.evaluate(() => (window.location.hash = '#/settings?tab=network'));
  await expect(selected).toHaveText('Network');
  await page.evaluate(() => (window.location.hash = '#/settings?tab=alerts'));
  await expect(selected).toHaveText('Alerts');
  await page.goBack();
  await expect(selected).toHaveText('Network');
});
