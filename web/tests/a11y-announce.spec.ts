import { test, expect, type Page } from '@playwright/test';

// Live readings don't flood screen readers (spec 022 SC-003, FR-009): the
// polite live region stays silent while values update every second, and a
// verdict change is announced exactly once.

const recordAnnouncements = (page: Page) =>
  page.evaluate(() => {
    const region = document.getElementById('sqm-live')!;
    const heard: string[] = [];
    (window as unknown as { heard: string[] }).heard = heard;
    new MutationObserver(() => {
      const text = region.textContent?.trim();
      if (text) heard.push(text);
    }).observe(region, { childList: true, characterData: true, subtree: true });
  });

const heard = (page: Page) => page.evaluate(() => (window as unknown as { heard: string[] }).heard);

test('the dashboard is silent while readings update, and announces a verdict change once', async ({ page }) => {
  test.setTimeout(90_000);
  await page.goto('./#/');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2500);
  await recordAnnouncements(page);

  // Readings tick every second; nothing should be said.
  await page.waitForTimeout(8000);
  expect(await heard(page)).toEqual([]);

  await page.getByRole('button', { name: /Demo/ }).click();
  await page.getByRole('button', { name: 'Rain', exact: true }).click();
  await expect(page.getByText('Rain detected').first()).toBeVisible({ timeout: 15_000 });
  await page.waitForTimeout(6000);

  const verdicts = (await heard(page)).filter((text) => text.startsWith('Observatory'));
  expect(verdicts).toHaveLength(1);
  expect(verdicts[0]).toMatch(/^Observatory unsafe/);
});
