import { test, expect, type Page } from '@playwright/test';

// Spec 019: the Demo panel sets what each sensor reports; the device's own code does the rest.

const ready = async (page: Page, path = '') => {
  await page.goto(`./${path}`);
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2500); // first ticks of the emulated device
};

const api = (page: Page, path: string, init?: RequestInit) =>
  page.evaluate(
    async ([p, i]) => {
      const response = await fetch(p as string, i as RequestInit | undefined);
      return { status: response.status, body: await response.text() };
    },
    [path, init] as const,
  );

const post = (page: Page, body: string) =>
  api(page, '/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body });
const json = async (page: Page, path: string) => JSON.parse((await api(page, path)).body);

const openPanel = async (page: Page, path = '') => {
  await ready(page, path);
  await page.getByRole('button', { name: /Demo/ }).click();
};

const setNumber = async (page: Page, label: string, value: number) => {
  const input = page.getByLabel(label, { exact: true });
  await input.fill(String(value));
  await input.press('Enter');
};

const openGroup = (page: Page, title: string) => page.locator('summary', { hasText: title }).click();

// The device's clock from /api/status ("2026-10-08T23:10:00+0100").
const deviceClock = async (page: Page) =>
  new Date((await json(page, '/api/status')).time.iso.replace(/([+-]\d{2})(\d{2})$/, '$1:$2')).getTime();

test('your thresholds decide what the readings mean (US1, SC-002)', async ({ page }) => {
  await openPanel(page);
  await post(page, '{"cloudDetection":{"clearSkyThreshold":-30,"cloudyThreshold":-20}}');
  await page.getByLabel(/Hold steady/).check();
  await openGroup(page, 'Air (BME280)');
  await setNumber(page, 'Air temperature (°C)', 20);
  await setNumber(page, 'Humidity (%)', 40);
  await setNumber(page, 'IR sensor temperature (°C)', 20);
  await setNumber(page, 'Sky temperature (°C)', -12);
  await expect.poll(async () => (await json(page, '/api/sensors')).clouds.condition, { timeout: 10_000 }).toBe('clear');
  await setNumber(page, 'Sky temperature (°C)', -5);
  await expect.poll(async () => (await json(page, '/api/sensors')).clouds.coverPercent, { timeout: 10_000 }).toBeGreaterThan(30);
  const clouds = (await json(page, '/api/sensors')).clouds;
  // The device's formula: (sky - IR sensor) - 0.75/100 × humidity; cover linear from -30 (0%) to -20 (100%).
  expect(clouds.correctedDelta).toBeCloseTo(-5 - 20 - 0.0075 * 40, 1);
  expect(clouds.coverPercent).toBeCloseTo(((clouds.correctedDelta + 30) / 10) * 100, 0);
});

test('Clear and Overcast are worked out from your thresholds (US3, SC-001)', async ({ page }) => {
  test.setTimeout(90_000);
  await openPanel(page);
  await post(page, '{"cloudDetection":{"clearSkyThreshold":-30,"cloudyThreshold":-20}}');
  await page.getByLabel('Change').selectOption('0');
  await page.getByRole('button', { name: 'Overcast', exact: true }).click();
  await expect(page.getByText(/your overcast threshold is -20.0 °C/)).toBeVisible();
  await expect.poll(async () => (await json(page, '/api/sensors')).clouds.condition, { timeout: 15_000 }).toBe('overcast');
  await page.getByRole('button', { name: 'Clear', exact: true }).click();
  await expect(page.getByText(/your clear-sky threshold is -30.0 °C/)).toBeVisible();
  await expect.poll(async () => (await json(page, '/api/sensors')).clouds.condition, { timeout: 15_000 }).toBe('clear');
});

test("a shortcut that can't apply says why and changes nothing (US3-2)", async ({ page }) => {
  await openPanel(page);
  await post(page, '{"alpaca":{"cloudCoverEnabled":false}}');
  const sky = await page.getByLabel('Sky temperature (°C)', { exact: true }).inputValue();
  await page.getByRole('button', { name: 'Cloud just unsafe' }).click();
  await expect(page.getByText(/The cloud cover safety rule is off/)).toBeVisible();
  expect(await page.getByLabel('Sky temperature (°C)', { exact: true }).inputValue()).toBe(sky);
  await page.getByRole('button', { name: 'Safety rules' }).click();
  await expect(page.getByRole('tab', { name: 'Safety', selected: true })).toBeVisible();
});

test('rain controls are unavailable while the rain sensor is off (FR-016)', async ({ page }) => {
  await openPanel(page);
  await post(page, '{"rain":{"enabled":false}}');
  await openGroup(page, 'Rain (RG-15)');
  await expect(page.getByLabel('Rain rate (mm/h)', { exact: true })).toBeDisabled({ timeout: 5000 });
  await page.getByRole('button', { name: 'Rain', exact: true }).click();
  await expect(page.getByText(/Rain: The rain sensor is switched off/)).toBeVisible();
});

test('waits are shown: rain clear delay and light averaging (US2)', async ({ page }) => {
  await openPanel(page);
  await openGroup(page, 'Rain (RG-15)');
  await setNumber(page, 'Rain rate (mm/h)', 2);
  await expect.poll(async () => (await json(page, '/api/safety')).safe, { timeout: 10_000 }).toBe(false);
  await setNumber(page, 'Rain rate (mm/h)', 0);
  await expect(page.getByText(/Rain clear delay - .* until rain is cleared/)).toBeVisible({ timeout: 5000 });
  await page.getByRole('button', { name: 'Darkest tonight' }).click();
  await setNumber(page, 'Illuminance (lux)', 0.002);
  await expect(page.getByText(/Sky brightness averages over 90 s - settled in/)).toBeVisible({ timeout: 5000 });
});

test('Darkest tonight moves the clock, Sun & Moon follow, and 10× runs it faster (US4)', async ({ page }) => {
  await openPanel(page);
  await page.getByRole('button', { name: 'Darkest tonight' }).click();
  await page.waitForTimeout(1500);
  expect((await json(page, '/api/status')).sky.sunAltitudeDeg).toBeLessThan(-12);
  await expect(page.getByText(/Dark now/)).toBeVisible({ timeout: 5000 });
  await page.getByLabel(/10× faster/).check();
  const t0 = await deviceClock(page);
  await page.waitForTimeout(3000);
  expect((await deviceClock(page)) - t0).toBeGreaterThan(20_000);
});

test('Dawn in London; no dawn at the North Pole on 31 December (SC-006)', async ({ page }) => {
  await openPanel(page);
  await page.getByRole('button', { name: 'London' }).click();
  await page.getByRole('button', { name: 'Dawn' }).click();
  await page.waitForTimeout(1500);
  const sun = (await json(page, '/api/status')).sky.sunAltitudeDeg;
  expect(sun).toBeGreaterThan(-12);
  expect(sun).toBeLessThan(-6);

  await page.getByRole('button', { name: 'North Pole' }).click();
  await page.getByRole('button', { name: '31 Dec 23:00' }).click();
  await page.waitForTimeout(1200);
  const before = await deviceClock(page);
  await page.getByRole('button', { name: 'Dawn' }).click();
  await expect(page.getByText(/no sunrise here/)).toBeVisible();
  expect(Math.abs((await deviceClock(page)) - before)).toBeLessThan(5000);
});

test('a GPS that stops answering is a GPS fault (FR-001)', async ({ page }) => {
  await openPanel(page);
  await post(page, '{"gps":{"enabled":true}}');
  await api(page, '/api/restart', { method: 'POST' });
  await page.waitForTimeout(6500);
  expect((await json(page, '/api/sensors')).gps.fix).toBe(true);
  await openGroup(page, 'GPS');
  await page.getByLabel('GPS not responding').check();
  await expect.poll(async () => (await json(page, '/api/sensors')).gps.status, { timeout: 5000 }).not.toBe('ok');
});

test('a place saves its location and time zone, as Settings does (FR-014)', async ({ page }) => {
  await openPanel(page);
  await page.getByRole('button', { name: 'Sydney' }).click();
  const config = await json(page, '/api/config');
  expect(config.location.latitude).toBeCloseTo(-33.87, 2);
  expect(config.ntp.timezone).toBe('AEST-10AEDT,M10.1.0,M4.1.0/3');
  expect((await json(page, '/api/status')).time.iso).toMatch(/\+1[01]00$/);
});

test('scenario links still work (FR-012, SC-005)', async ({ page }) => {
  test.setTimeout(120_000);
  await ready(page, '?scenario=rain');
  expect((await json(page, '/api/safety')).reasons.join(' ')).toContain('Rain');
  await page.evaluate(() => sessionStorage.clear());
  await ready(page, '?scenario=fail-ir');
  expect((await json(page, '/api/sensors')).infrared.status).not.toBe('ok');
  await page.evaluate(() => sessionStorage.clear());
  await ready(page, '?scenario=night');
  expect((await json(page, '/api/status')).sky.sunAltitudeDeg).toBeLessThan(-12);
  await page.evaluate(() => sessionStorage.clear());
  await ready(page, '?scenario=cloud');
  await expect.poll(async () => (await json(page, '/api/safety')).reasons.join(' '), { timeout: 60_000 }).toMatch(/Cloud/);
});

test('on a phone nothing hides a Save button, and the panel scrolls and closes (US5, SC-007)', async ({ page }) => {
  await page.setViewportSize({ width: 375, height: 667 });
  for (const tab of ['device', 'network', 'time', 'sensors', 'safety', 'alerts']) {
    await ready(page, `#/settings?tab=${tab}`);
    const save = page.getByRole('button', { name: 'Save' });
    if ((await save.count()) === 0) continue;
    await save.scrollIntoViewIfNeeded();
    const a = (await save.boundingBox())!;
    const b = (await page.locator('.demo-panel-toggle').boundingBox())!;
    const overlap = a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
    expect(overlap, `Save on ${tab}`).toBe(false);
  }
  await page.getByRole('button', { name: /Demo/ }).click();
  const body = page.locator('.demo-panel-body');
  expect(await body.evaluate((el) => el.scrollHeight > el.clientHeight)).toBe(true);
  await body.evaluate((el) => el.scrollTo(0, el.scrollHeight));
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(375);
  await page.getByRole('button', { name: 'Close' }).click();
  await expect(body).toHaveCount(0);
});

test('every control stays inside the browser (SC-008)', async ({ page }) => {
  test.setTimeout(120_000);
  const outside: string[] = [];
  page.on('request', (request) => {
    const url = new URL(request.url());
    if (url.protocol.startsWith('http') && url.hostname !== 'localhost') outside.push(request.url());
  });
  await openPanel(page);
  for (const name of ['Clear', 'Overcast', 'Cloud just unsafe', 'Rain', 'Rain stops', 'Dark sky', 'Dew risk'])
    await page.getByRole('button', { name, exact: true }).click();
  for (const name of ['Now', 'Darkest tonight', 'Dawn', 'Dusk', 'Midsummer midnight', 'Midwinter midnight', '31 Dec 23:00'])
    await page.getByRole('button', { name, exact: true }).click();
  for (const name of ['La Palma', 'Atacama', 'Sydney', '75° N 1° W', 'North Pole', 'London'])
    await page.getByRole('button', { name, exact: true }).click();
  for (const title of ['Air (BME280)', 'Rain (RG-15)', 'Wind', 'GPS']) await openGroup(page, title);
  for (const checkbox of await page.locator('.demo-panel-body input[type=checkbox]').all())
    if (await checkbox.isEnabled()) await checkbox.click();
  for (const input of await page.locator('.demo-panel-body input[type=number]').all()) {
    if (!(await input.isEnabled())) continue;
    await input.fill('1');
    await input.press('Enter');
  }
  await page.waitForTimeout(1500);
  expect(outside).toEqual([]);
});
