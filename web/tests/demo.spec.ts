import { test, expect, type Page } from '@playwright/test';

// The demo as one emulated device (specs/016-demo-device-emulation).

const ready = async (page: Page, hash = '') => {
  await page.goto(`./${hash}`);
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

test.describe('demo links (US1, SC-001)', () => {
  test('every page renders and every link stays in the demo', async ({ page }) => {
    for (const hash of ['', '#/alpaca', '#/system', '#/settings', '#/updates', '#/wifi']) {
      await ready(page, hash);
      await expect(page.getByText('Page not found')).toHaveCount(0);
    }
    await ready(page, '#/alpaca');
    const links = await page.$$eval('main a[href]', (as) => as.map((a) => (a as HTMLAnchorElement).href));
    expect(links.length).toBeGreaterThan(5);
    for (const href of links) {
      const url = new URL(href);
      expect(url.origin).toBe(new URL(page.url()).origin);
      await page.goto(href);
      await page.waitForTimeout(1500);
      const text = await page.locator('body').innerText();
      expect(text).not.toContain("There isn't a GitHub Pages site here");
      expect(text).not.toContain('Page not found');
    }
  });

  test("device URLs opened directly show the device's answer", async ({ page }) => {
    await page.goto('./management/v1/description');
    await expect(page.getByText('"ServerName": "SQMeter"')).toBeVisible();
    await page.goto('./api/v1/safetymonitor/0/issafe');
    await expect(page.getByText('"Value"')).toBeVisible();
    await page.goto('./setup/v1/safetymonitor/0/setup');
    await expect(page.getByRole('tab', { name: 'Safety', selected: true })).toBeVisible();
  });
});

test('rain: dashboard, Alpaca page and Alpaca IsSafe agree (US1/US5, SC-002)', async ({ page }) => {
  await ready(page);
  await page.getByRole('button', { name: /Demo/ }).click();
  await page.getByRole('button', { name: 'Rain', exact: true }).click();
  await expect(page.getByText('Rain detected').first()).toBeVisible({ timeout: 15000 });
  const issafe = await api(page, '/api/v1/safetymonitor/0/issafe');
  expect(JSON.parse(issafe.body).Value).toBe(false);
  const safety = JSON.parse((await api(page, '/api/safety')).body);
  expect(safety.safe).toBe(false);
  expect(safety.reasons.join(' ')).toContain('Rain');
  await page.goto('./#/alpaca');
  await expect(page.getByText('Rain detected').first()).toBeVisible({ timeout: 10000 });
  await expect(page.getByText(/IsSafe\s*false/)).toBeVisible({ timeout: 10000 });
});

test('settings change the emulated device (US2, SC-003)', async ({ page }) => {
  await ready(page);
  // Rejected with the device's own message.
  const bad = await api(page, '/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: '{"wifi":{"hostname":"-x"}}',
  });
  expect(bad.status).toBe(400);
  expect(JSON.parse(bad.body).error).toContain('Hostname');

  // Rain sensor off: gone from the readings.
  await api(page, '/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{"rain":{"enabled":false}}' });
  await page.waitForTimeout(1500);
  expect(JSON.parse((await api(page, '/api/sensors')).body).rain).toBeUndefined();

  // GPS needs a restart, like the device.
  await api(page, '/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{"gps":{"enabled":true}}' });
  await page.waitForTimeout(1500);
  expect(JSON.parse((await api(page, '/api/sensors')).body).gps.status).toBe('missing');
  await api(page, '/api/restart', { method: 'POST' });
  await page.waitForTimeout(6500);
  expect(JSON.parse((await api(page, '/api/sensors')).body).gps.fix).toBe(true);

  // A location drives darkness at the device.
  await api(page, '/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: '{"location":{"set":true,"latitude":-33.86,"longitude":151.21}}',
  });
  await page.waitForTimeout(1500);
  const sky = JSON.parse((await api(page, '/api/status')).body).sky;
  expect(sky.locationSource).toBe('gps'); // the GPS fix wins over settings
});

test('nothing leaves the browser (US3, SC-004)', async ({ page }) => {
  const outside: string[] = [];
  page.on('request', (request) => {
    const url = new URL(request.url());
    if (url.protocol.startsWith('http') && url.origin !== 'http://localhost:4173') outside.push(request.url());
  });
  await ready(page);
  const post = (path: string, body?: string) => api(page, path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body });
  await post(
    '/api/config',
    '{"alerts":{"enabled":true,"pushover":{"enabled":true,"userKey":"u12345678901234567890123456789","appToken":"a12345678901234567890123456789"},"webhook":{"enabled":true,"url":"https://example.com/hook"}}}',
  );
  await post('/api/alerts/test?channel=all');
  await post('/api/alerts/test?channel=pushover&event=rain_started&level=4');
  await post('/api/mqtt/test', '{"broker":"test.mosquitto.org","port":1883}');
  await api(page, '/api/updates/check?track=beta');
  await post('/api/updates/apply', '{}');
  await api(page, '/api/wifi/scan');
  await post('/api/wifi/connect', '{"ssid":"x","password":"y"}');
  for (const hash of ['#/settings?tab=alerts', '#/updates', '#/wifi', '#/system']) await ready(page, hash);
  const recent = JSON.parse((await api(page, '/api/alerts/recent')).body);
  expect(recent.alerts[0].channels.pushover.detail).toBe('Demo: nothing was sent');
  expect(outside).toEqual([]);
});

test.describe("demo scenarios follow the device", () => {
  const openPanel = async (page: Page) => {
    await ready(page);
    await page.getByRole("button", { name: /Demo/ }).click();
  };
  const status = async (page: Page) => JSON.parse((await api(page, "/api/status")).body);

  test("rain scenarios are off while the rain sensor is", async ({ page }) => {
    await openPanel(page);
    await expect(page.getByRole("button", { name: "Rain", exact: true })).toBeEnabled();
    await api(page, "/api/config", { method: "POST", headers: { "Content-Type": "application/json" }, body: '{"rain":{"enabled":false}}' });
    await expect(page.getByRole("button", { name: "Rain", exact: true })).toBeDisabled({ timeout: 5000 });
    await expect(page.getByRole("button", { name: "Rain sensor fails" })).toBeDisabled();
    await page.getByRole("button", { name: "Sensors" }).click();
    await expect(page.getByRole("tab", { name: "Sensors", selected: true })).toBeVisible();
  });

  test("Night sky moves the device clock, and Sun & Moon follow it", async ({ page }) => {
    await openPanel(page);
    const before = new Date((await status(page)).time.iso).getTime();
    await page.getByRole("button", { name: "Night sky" }).click();
    await page.waitForTimeout(2500);
    const night = await status(page);
    expect(night.sky.sunAltitudeDeg).toBeLessThan(-12); // the darkest moment tonight
    expect(Math.abs(new Date(night.time.iso).getTime() - before)).toBeGreaterThan(0);
    await expect(page.getByText(/Dark now/)).toBeVisible({ timeout: 5000 });

    // 10x runs the device's clock 10x too.
    await page.getByLabel(/10× faster/).check();
    const t0 = new Date((await status(page)).time.iso).getTime();
    await page.waitForTimeout(3000);
    const t1 = new Date((await status(page)).time.iso).getTime();
    expect(t1 - t0).toBeGreaterThan(20_000);
  });

  test("Cloud over clouds the sky past the unsafe limit", async ({ page }) => {
    test.setTimeout(120_000);
    await openPanel(page);
    await page.getByRole("button", { name: "Cloud over" }).click();
    await expect
      .poll(async () => JSON.parse((await api(page, "/api/sensors")).body).clouds.coverPercent, { timeout: 30_000 })
      .toBeGreaterThan(20);
    await expect.poll(async () => JSON.parse((await api(page, "/api/safety")).body).safe, { timeout: 60_000 }).toBe(false);
    const safety = JSON.parse((await api(page, "/api/safety")).body);
    expect(safety.reasons.join(" ").toLowerCase()).toContain("cloud");
  });

  test("Dawn moves the device clock to just before sunrise", async ({ page }) => {
    await openPanel(page);
    await page.getByRole("button", { name: "Dawn" }).click();
    await page.waitForTimeout(2500);
    const sun = (await status(page)).sky.sunAltitudeDeg;
    expect(sun).toBeGreaterThan(-13);
    expect(sun).toBeLessThan(-10);
  });

  test("on a phone the Demo button leaves the Save button clear", async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    await ready(page, "#/settings?tab=device");
    await page.locator("input").first().fill("Changed name");
    const save = page.getByRole("button", { name: "Save" });
    await expect(save).toBeVisible();
    const a = (await save.boundingBox())!;
    const b = (await page.locator(".demo-panel-toggle").boundingBox())!;
    const overlap = a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
    expect(overlap).toBe(false);
  });

  test("Cloud over and Clear follow the cloud detection settings", async ({ page }) => {
    test.setTimeout(120_000);
    await openPanel(page);
    await api(page, "/api/config", { method: "POST", headers: { "Content-Type": "application/json" }, body: '{"cloudDetection":{"clearSkyThreshold":-30,"cloudyThreshold":-20}}' });
    await page.waitForTimeout(3000);
    expect(JSON.parse((await api(page, "/api/sensors")).body).clouds.condition).toBe("clear");
    await page.getByRole("button", { name: "Cloud over" }).click();
    await expect.poll(async () => JSON.parse((await api(page, "/api/sensors")).body).clouds.condition, { timeout: 60_000 }).toBe("overcast");
  });
});
