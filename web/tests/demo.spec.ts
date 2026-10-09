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
  await expect(page.locator('.reason-list').getByText('Rain detected')).toBeVisible({ timeout: 15000 });
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

test('nothing leaves the browser (US3, SC-004)', async ({ page, baseURL }) => {
  const outside: string[] = [];
  const own = new URL(baseURL ?? 'http://localhost:4173/').origin;
  page.on('request', (request) => {
    const url = new URL(request.url());
    if (url.protocol.startsWith('http') && url.origin !== own) outside.push(request.url());
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

test.describe('the imaging app (specs/021)', () => {
  const recentEvents = async (page: Page) =>
    (JSON.parse((await api(page, '/api/alerts/recent')).body).alerts as { event: string }[]).map((a) => a.event);

  test('a silent imaging app is noticed, its return too; a disconnect pauses alerts', async ({ page }) => {
    test.setTimeout(150_000);
    await ready(page);
    await page.getByRole('button', { name: /Demo/ }).click();
    await page.getByLabel(/10× faster/).check();
    const app = page.getByLabel('Imaging app');
    await app.getByRole('button', { name: 'Connect' }).click();
    await expect(app.getByText(/Connected - checking/)).toBeVisible();
    await page.waitForTimeout(2000);
    const status = JSON.parse((await api(page, '/api/status')).body);
    expect(status.alpaca.clients.safetymonitor.connected).toBe(true);

    // Silent for 2 min of device time: about 12 s at 10x.
    await app.getByRole('button', { name: 'Go silent' }).click();
    await expect.poll(() => recentEvents(page), { timeout: 40_000 }).toContain('client_lost');

    // Back: sent once the cooldown (5 min device time) has run.
    await app.getByRole('button', { name: 'Resume checking' }).click();
    await expect.poll(() => recentEvents(page), { timeout: 60_000 }).toContain('client_back');

    // Only while an imaging app is connected: a clean disconnect pauses alerts.
    await api(page, '/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{"alerts":{"sendMode":"whileConnected"}}',
    });
    await page.waitForTimeout(1500);
    await app.getByRole('button', { name: 'Disconnect' }).click();
    await expect
      .poll(async () => JSON.parse((await api(page, '/api/alerts/armed')).body), { timeout: 10_000 })
      .toMatchObject({ armed: false, mode: 'whileConnected', reason: 'client-disconnected' });
    await page.getByRole('button', { name: /Demo/ }).first().click(); // close the panel
    await page.goto('./#/settings?tab=alerts');
    await expect(page.getByText(/Paused - the imaging app disconnected/)).toBeVisible({ timeout: 10_000 });
  });
});

// The demo's readings look like a real observatory's (spec 010 FR-005 / SC-002).
// At night: by day the sky is honestly bright (SQM near 0).
test('demo readings are plausible', async ({ page }) => {
  await ready(page, '?scenario=night');
  await page.waitForTimeout(3000); // the sky average settles
  const sensors = JSON.parse((await api(page, '/api/sensors')).body);
  const status = JSON.parse((await api(page, '/api/status')).body);
  const within = (value: number, min: number, max: number, what: string) => {
    expect(value, what).toBeGreaterThanOrEqual(min);
    expect(value, what).toBeLessThanOrEqual(max);
  };
  within(sensors.sky.sqm, 15, 22.5, 'SQM');
  within(sensors.sky.bortle, 1, 9, 'Bortle');
  within(sensors.environment.temperature, -40, 50, 'air temperature');
  within(sensors.environment.humidity, 0, 100, 'humidity');
  within(sensors.environment.pressure, 870, 1085, 'pressure');
  within(sensors.environment.dewpoint, -60, sensors.environment.temperature, 'dew point');
  within(sensors.infrared.skyTemperature, -60, sensors.infrared.ambientTemperature, 'sky temperature');
  within(sensors.clouds.coverPercent, 0, 100, 'cloud cover');
  within(sensors.wind.speed, 0, 60, 'wind speed');
  within(sensors.wind.gust, sensors.wind.speed, 80, 'wind gust');
  within(status.uptime, 0, 30 * 24 * 3600, 'uptime');
  within(status.wifi.rssi, -100, -20, 'WiFi signal');
});
