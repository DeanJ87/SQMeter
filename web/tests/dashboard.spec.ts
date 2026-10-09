import { test, expect, type Page } from '@playwright/test';

// One test per item of web/src/dashboard/inventory.json (specs/025 FR-003):
// drive the demo device into the state, check the item shows, then that it
// hides when its rule says so. tools/dashboard/check.py fails if a shown entry
// has no `inventory: <id>` here.

type Patch = Record<string, unknown>;

const open = async (page: Page, query = '') => {
  await page.goto(`./${query}#/`);
  await expect(page.locator('.glance')).toBeVisible({ timeout: 15_000 });
};

// Change the demo device's settings the way the Settings page does, then
// reload. Keys are paths; `*` matches every key at that level.
const setConfig = async (page: Page, changes: Patch) => {
  await page.evaluate(async (changes) => {
    const config = await (await fetch('/api/config')).json();
    const assign = (node: Record<string, unknown>, path: string[], value: unknown) => {
      const [head, ...rest] = path;
      const keys = head === '*' ? Object.keys(node) : [head];
      for (const key of keys) {
        if (!rest.length) node[key] = value;
        else assign((node[key] ??= {}) as Record<string, unknown>, rest, value);
      }
    };
    for (const [path, value] of Object.entries(changes)) assign(config, path.split('.'), value);
    await fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(config) });
  }, changes);
  await page.reload();
  await expect(page.locator('.glance')).toBeVisible({ timeout: 15_000 });
};

const item = (page: Page, id: string) => page.locator(`[data-inventory="${id}"]`);
const line = (page: Page) => page.locator('.glance-line');
const card = (page: Page, title: string) =>
  page.locator('.masonry-item').filter({ has: page.getByRole('heading', { name: title, exact: true }) });

const openPanel = async (page: Page) => {
  const toggle = page.locator('.demo-panel-toggle');
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.click();
};

const imagingApp = async (page: Page, button: string) => {
  await openPanel(page);
  await page.locator('[aria-label="Imaging app"]').getByRole('button', { name: button }).click();
};

test.describe.configure({ retries: 0 });

test('healthy: one line - Live, Safe, Sending alerts', async ({ page }) => {
  // inventory: freshness  inventory: safety-verdict  inventory: alerts-state
  await open(page);
  await expect(line(page)).toContainText('Live');
  await expect(line(page)).toContainText('Safe');
  await expect(line(page)).toContainText('Sending alerts');
  await expect(page.locator('.glance-problem')).toHaveCount(0);
});

test('unsafe verdict and a held rain countdown', async ({ page }) => {
  // inventory: safety-verdict  inventory: rain-hold
  await open(page);
  await expect(item(page, 'rain-hold')).toHaveCount(0);
  await open(page, '?scenario=rain');
  await expect(page.locator('.glance-problem').filter({ hasText: 'Unsafe' })).toBeVisible({ timeout: 15_000 });
  await openPanel(page);
  await page.locator('.demo-panel').getByRole('button', { name: 'Rain stops', exact: true }).click();
  await expect(item(page, 'rain-hold')).toContainText(/Rain held - clears in/, { timeout: 15_000 });
});

test('paused alerts show with Resume, then go', async ({ page }) => {
  // inventory: alerts-state
  await open(page);
  await page.evaluate(() => fetch('/api/alerts/disarm?source=ui', { method: 'POST' }));
  const paused = page.locator('.glance-item').filter({ hasText: 'Paused by you' });
  await expect(paused).toBeVisible({ timeout: 10_000 });
  await paused.getByRole('button', { name: 'Resume' }).click();
  await expect(paused).toHaveCount(0, { timeout: 10_000 });
  await expect(line(page)).toContainText('Sending alerts');
});

test('send mode not in effect while Alpaca is off', async ({ page }) => {
  // inventory: alerts-mode-not-in-effect
  await open(page);
  await setConfig(page, { 'alerts.sendMode': 'whileConnected', 'alpaca.enabled': false });
  await expect(item(page, 'alerts-mode-not-in-effect')).toContainText('Alpaca is off');
  await setConfig(page, { 'alpaca.enabled': true });
  await expect(item(page, 'alerts-mode-not-in-effect')).toHaveCount(0);
});

test('imaging app: waiting, watching, then stopped checking', async ({ page }) => {
  // inventory: imaging-app
  test.setTimeout(120_000);
  await open(page);
  await expect(item(page, 'imaging-app')).toHaveCount(0);
  await setConfig(page, { 'alerts.sendMode': 'whileConnected', 'alerts.clientSilentSafetySeconds': 30 });
  await expect(item(page, 'alerts-state')).toContainText('Waiting for an imaging app');
  await expect(item(page, 'imaging-app')).toHaveCount(2);
  await imagingApp(page, 'Connect');
  await expect(line(page)).toContainText('Imaging app (safety monitor): Connected', { timeout: 15_000 });
  await imagingApp(page, 'Go silent');
  await expect(page.locator('.glance-problem').filter({ hasText: 'Imaging app (safety monitor)' })).toContainText('Gone quiet', {
    timeout: 60_000,
  });
});

test('no alert channel can send', async ({ page }) => {
  // inventory: no-channel
  await open(page);
  await expect(item(page, 'no-channel')).toHaveCount(0);
  await setConfig(page, { 'alerts.ntfy.enabled': false, 'alerts.mqtt.enabled': true, 'mqtt.enabled': false });
  await expect(item(page, 'no-channel')).toContainText('No alert channel can send');
});

test('a failed sensor keeps its card and is a problem', async ({ page }) => {
  // inventory: sensor-faults  inventory: cloud-card
  await open(page);
  await expect(card(page, 'Cloud Conditions')).not.toContainText('Error');
  await open(page, '?scenario=fail-ir');
  await expect(page.locator('.glance-problem').filter({ hasText: 'IR sky sensor (MLX90614)' })).toBeVisible({ timeout: 15_000 });
  await expect(card(page, 'Cloud Conditions')).toContainText("Cloud cover and the cloud safety rule can't be measured.");
});

test('settings not in effect are summarised', async ({ page }) => {
  // inventory: settings-not-in-effect
  await open(page);
  await expect(item(page, 'settings-not-in-effect')).toContainText('1 setting not in effect');
  await setConfig(page, { 'alerts.events.*.level': 2 });
  await expect(item(page, 'settings-not-in-effect')).toHaveCount(0);
});

test('no location: darkness unknown', async ({ page }) => {
  // inventory: clock-location
  await open(page);
  await expect(item(page, 'clock-location')).toHaveCount(0);
  await setConfig(page, { 'location.set': false, 'gps.enabled': false });
  await expect(item(page, 'clock-location')).toContainText('No location', { timeout: 10_000 });
});

test('safety rules not in effect and not shared with N.I.N.A.', async ({ page }) => {
  // inventory: rules-not-in-effect  inventory: safety-not-shared
  await open(page);
  await expect(item(page, 'rules-not-in-effect')).toHaveCount(0);
  await expect(item(page, 'safety-not-shared')).toHaveCount(0);
  await setConfig(page, { 'rain.enabled': false, 'alpaca.rainUnsafeEnabled': true, 'alpaca.enabled': false });
  await expect(item(page, 'rules-not-in-effect')).toContainText('rain sensor is off');
  await expect(item(page, 'safety-not-shared')).toBeVisible();
});

test('update available from this session’s check', async ({ page }) => {
  // inventory: update-available
  await open(page);
  await expect(item(page, 'update-available')).toHaveCount(0);
  await page.evaluate(() => sessionStorage.setItem('sqm.updates.latest', 'v9.9.9'));
  await page.reload();
  await expect(item(page, 'update-available')).toContainText('Update available: v9.9.9');
});

test('demo marker, and the moved demo clock', async ({ page }) => {
  // inventory: demo-marker
  await open(page);
  await expect(item(page, 'demo-marker')).toHaveText('Demo');
  await open(page, '?scenario=night');
  await expect(item(page, 'demo-marker')).toContainText('device clock moved');
});

test('the readings cards', async ({ page }) => {
  // inventory: sky-card  inventory: light-card  inventory: cloud-card  inventory: environment-card
  // inventory: rain-card  inventory: wind-card  inventory: device-network-card
  await open(page);
  for (const title of ['Sky Quality', 'Light Sensor', 'Cloud Conditions', 'Environment', 'Rain Sensor', 'Wind', 'Device & Network'])
    await expect(card(page, title)).toBeVisible();
  await setConfig(page, { 'rain.enabled': false, 'wind.enabled': false });
  await expect(card(page, 'Rain Sensor')).toHaveCount(0);
  await expect(card(page, 'Wind')).toHaveCount(0);
});

test('GPS card only when GPS is on', async ({ page }) => {
  // inventory: gps-card
  await open(page);
  await expect(card(page, 'GPS Location')).toHaveCount(0);
  await setConfig(page, { 'gps.enabled': true });
  await expect(card(page, 'GPS Location')).toBeVisible({ timeout: 10_000 });
});

test('Device & Network: IPv6, local name and MQTT', async ({ page }) => {
  // inventory: ipv6  inventory: mdns  inventory: mqtt-state
  await open(page);
  await expect(item(page, 'ipv6')).toBeVisible();
  await expect(item(page, 'mdns')).toContainText('.local');
  await expect(item(page, 'mqtt-state')).toContainText('Connected');
  await setConfig(page, { 'wifi.ipv6': false, 'wifi.mdns': false, 'mqtt.enabled': false });
  await expect(item(page, 'ipv6')).toHaveCount(0);
  await expect(item(page, 'mdns')).toHaveCount(0);
  await expect(item(page, 'mqtt-state')).toHaveCount(0);
});
