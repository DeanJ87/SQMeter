import { test, expect, type Page } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

// The demo tour (specs/018 US1). These start from a clean browser, so the
// first-visit offer shows; other suites preload "dismissed".
test.use({ storageState: { cookies: [], origins: [] } });

const AXE_TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];

const open = async (page: Page) => {
  await page.goto('./');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2000); // first ticks of the emulated device
};

const card = (page: Page) => page.getByRole('dialog', { name: /./ }).filter({ has: page.locator('.tour-progress') });

// Walk the whole tour; action steps are done with "Do it for me".
async function walk(page: Page, eachStep?: () => Promise<void>) {
  for (;;) {
    await expect(card(page)).toBeVisible();
    if (eachStep) await eachStep();
    const doIt = card(page).getByRole('button', { name: /for me/ });
    if (await doIt.isVisible()) await doIt.click();
    const finish = card(page).getByRole('button', { name: 'Finish' });
    if (await finish.isVisible()) {
      await finish.click();
      return;
    }
    await expect(card(page).getByRole('button', { name: 'Next' })).toBeEnabled({ timeout: 15_000 });
    await card(page).getByRole('button', { name: 'Next' }).click();
  }
}

test('offered once on a first visit, walks through with the device reacting, not offered again', async ({ page }) => {
  await open(page);
  const offer = page.getByRole('region', { name: 'Tour' });
  await expect(offer).toBeVisible();
  await offer.getByRole('button', { name: 'Take the tour' }).click();
  await expect(card(page).getByRole('heading', { name: 'This is SQMeter' })).toBeFocused();

  const seen: string[] = [];
  await walk(page, async () => {
    const title = (await card(page).getByRole('heading').textContent()) ?? '';
    seen.push(title);
    if (title === 'Make it unsafe') {
      // An action step waits for the device, not a timer.
      await expect(card(page).getByRole('button', { name: 'Next' })).toBeDisabled();
    }
  });
  expect(seen).toContain('Make it unsafe');
  expect(seen.length).toBeGreaterThanOrEqual(6);
  expect(seen.length).toBeLessThanOrEqual(10);
  const safety = await page.evaluate(async () => (await fetch('/api/safety')).json());
  expect(safety.safe !== undefined).toBe(true);

  await expect(card(page)).toHaveCount(0);
  await page.reload();
  await page.waitForTimeout(1500);
  await expect(page.getByRole('region', { name: 'Tour' })).toHaveCount(0);
});

test('Escape ends it; the Demo panel starts it again', async ({ page }) => {
  await open(page);
  await page.getByRole('region', { name: 'Tour' }).getByRole('button', { name: 'No thanks' }).click();
  await page.getByRole('button', { name: /Demo/ }).click();
  await page.getByRole('button', { name: 'Take the tour' }).click();
  await expect(card(page)).toBeVisible();
  await page.keyboard.press('Escape');
  await expect(card(page)).toHaveCount(0);
});

test('keyboard only: Tab reaches the buttons and Enter moves on', async ({ page }) => {
  await open(page);
  await page.getByRole('region', { name: 'Tour' }).getByRole('button', { name: 'Take the tour' }).click();
  await expect(card(page).getByRole('heading', { name: 'This is SQMeter' })).toBeFocused();
  await page.keyboard.press('Tab');
  await expect(card(page).getByRole('button', { name: 'Next' })).toBeFocused();
  await page.keyboard.press('Enter');
  await expect(card(page).getByRole('heading', { name: 'Live readings' })).toBeFocused();
});

test('on a phone every step fits on screen', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page);
  await page.getByRole('region', { name: 'Tour' }).getByRole('button', { name: 'Take the tour' }).click();
  await walk(page, async () => {
    const box = (await card(page).boundingBox())!;
    expect(box.x).toBeGreaterThanOrEqual(0);
    expect(box.y).toBeGreaterThanOrEqual(0);
    expect(box.x + box.width).toBeLessThanOrEqual(390);
    expect(box.y + box.height).toBeLessThanOrEqual(844);
  });
});

test('the tour card passes the accessibility checks', async ({ page }) => {
  await open(page);
  await page.getByRole('region', { name: 'Tour' }).getByRole('button', { name: 'Take the tour' }).click();
  await expect(card(page)).toBeVisible();
  const results = await new AxeBuilder({ page }).include('.tour-card').withTags(AXE_TAGS).analyze();
  expect(results.violations.map((v) => v.id)).toEqual([]);
});
