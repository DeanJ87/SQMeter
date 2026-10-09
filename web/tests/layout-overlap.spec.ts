import { test, expect, type Locator, type Page } from '@playwright/test';

// Nothing floating hides a page control on a phone (specs/019 SC-007,
// specs/022 no obscured controls): the Demo button, the first-visit tour offer
// and the docked tour card must leave the settings save bar and toasts clear.
// Starts from a clean browser so the tour offer shows.
test.use({ storageState: { cookies: [], origins: [] } });

const overlaps = async (a: Locator, b: Locator) => {
  const x = await a.boundingBox();
  const y = await b.boundingBox();
  if (!x || !y) return false;
  return x.x < y.x + y.width && y.x < x.x + x.width && x.y < y.y + y.height && y.y < x.y + x.height;
};

const floating = (page: Page) => [page.locator('.demo-panel-toggle'), page.locator('.tour-offer'), page.locator('.tour-card')];

const expectClear = async (page: Page, control: Locator) => {
  await expect(control).toBeVisible();
  for (const layer of floating(page)) {
    if (await layer.isVisible()) expect(await overlaps(control, layer)).toBe(false);
  }
};

const openSettings = async (page: Page, width: number) => {
  await page.setViewportSize({ width, height: 700 });
  await page.goto('./#/settings?tab=device');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2000); // first ticks of the emulated device
};

for (const width of [320, 390]) {
  test(`at ${width} px the save bar and toasts stay clear of the Demo button and the tour offer`, async ({ page }) => {
    await openSettings(page, width);
    await expect(page.getByRole('region', { name: 'Tour' })).toBeVisible();

    await page.locator('[data-field="deviceName"]').fill('Changed name');
    const save = page.getByRole('button', { name: 'Save' });
    await expectClear(page, save);
    await save.click(); // a covered button fails here: the click is intercepted
    await expectClear(page, page.locator('.toast').first());
  });

  test(`at ${width} px the docked tour card leaves the save bar clear`, async ({ page }) => {
    await openSettings(page, width);
    await page.getByRole('region', { name: 'Tour' }).getByRole('button', { name: 'Take the tour' }).click();
    await expect(page.locator('.tour-card')).toBeVisible();
    await page.goto('./#/settings?tab=device');
    await expect(page.locator('.tour-card')).toBeVisible();
    await page.locator('[data-field="deviceName"]').fill('Changed again');
    await expectClear(page, page.getByRole('button', { name: 'Save' }));
  });
}
