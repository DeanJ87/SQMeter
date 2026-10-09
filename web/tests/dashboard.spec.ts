import { test, expect, type Page } from '@playwright/test';

// One test per item of web/src/dashboard/inventory.json (specs/025 FR-003,
// 026 FR-010): drive the demo device into the state, check the item shows,
// then that it hides when its rule says so. tools/dashboard/check.py fails if
// a shown entry has no `inventory: <id>` here.

type Patch = Record<string, unknown>;

const statusCard = (page: Page) => page.locator('[data-inventory="status-card"]');

const open = async (page: Page, query = '') => {
  await page.goto(`./${query}#/`);
  await expect(statusCard(page)).toBeVisible({ timeout: 15_000 });
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
  await expect(statusCard(page)).toBeVisible({ timeout: 15_000 });
};

const item = (page: Page, id: string) => page.locator(`[data-inventory="${id}"]`);
const tile = (page: Page, id: string) => statusCard(page).locator(`.status-tile[data-inventory="${id}"]`);
const pill = (page: Page) => statusCard(page).locator('.card-actions .pill');
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

test('healthy: a Status card like the others - Data, Safety, Alerts tiles, All good', async ({ page }) => {
  // inventory: status-card  inventory: freshness  inventory: safety-verdict  inventory: alerts-state
  await open(page);
  await expect(statusCard(page).getByRole('heading', { name: 'Status', exact: true })).toBeVisible();
  // The demo starts with one alert setting not in effect; make every setting work.
  await expect(pill(page)).toHaveText('1 to check');
  await setConfig(page, { 'alerts.events.*.level': 2 });
  await expect(pill(page)).toHaveText('All good');
  await expect(tile(page, 'freshness')).toContainText('Live');
  await expect(tile(page, 'safety-verdict')).toContainText('Safe');
  await expect(tile(page, 'alerts-state')).toContainText('Sending');
  await expect(statusCard(page).locator('.status-row')).toHaveCount(0);
  // It is the first card, and nothing sits between the header and the cards (026 FR-014).
  await expect(page.locator('.masonry-item').first()).toContainText('Status');
});

test('unsafe verdict and a held rain countdown', async ({ page }) => {
  // inventory: safety-verdict  inventory: rain-hold
  await open(page);
  await expect(item(page, 'rain-hold')).toHaveCount(0);
  await open(page, '?scenario=rain');
  await expect(tile(page, 'safety-verdict')).toContainText('Unsafe', { timeout: 15_000 });
  await expect(pill(page)).toHaveText(/to check/);
  await openPanel(page);
  await page.locator('.demo-panel').getByRole('button', { name: 'Rain stops', exact: true }).click();
  await expect(item(page, 'rain-hold')).toContainText(/Rain held, clears in/, { timeout: 15_000 });
});

test('paused alerts show on the Alerts tile with Resume, then go', async ({ page }) => {
  // inventory: alerts-state
  await open(page);
  await page.evaluate(() => fetch('/api/alerts/disarm?source=ui', { method: 'POST' }));
  await expect(tile(page, 'alerts-state')).toContainText('Paused', { timeout: 10_000 });
  await expect(tile(page, 'alerts-state')).toContainText('Paused by you');
  await tile(page, 'alerts-state').getByRole('button', { name: 'Resume' }).click();
  await expect(tile(page, 'alerts-state')).toContainText('Sending', { timeout: 10_000 });
});

test('send mode not in effect while Alpaca is off', async ({ page }) => {
  // inventory: alerts-mode-not-in-effect
  await open(page);
  await setConfig(page, { 'alerts.sendMode': 'whileConnected', 'alpaca.enabled': false });
  await expect(item(page, 'alerts-mode-not-in-effect')).toContainText('Send mode');
  await expect(item(page, 'alerts-mode-not-in-effect')).toContainText('Not in effect');
  await setConfig(page, { 'alpaca.enabled': true });
  await expect(item(page, 'alerts-mode-not-in-effect')).toHaveCount(0);
});

test('imaging app: waiting, connected, then gone quiet', async ({ page }) => {
  // inventory: imaging-app
  test.setTimeout(120_000);
  await open(page);
  await expect(item(page, 'imaging-app')).toHaveCount(0);
  await setConfig(page, { 'alerts.sendMode': 'whileConnected', 'alerts.clientSilentSafetySeconds': 30 });
  await expect(tile(page, 'alerts-state')).toContainText('Waiting');
  await expect(item(page, 'imaging-app')).toHaveCount(2);
  await imagingApp(page, 'Connect');
  await expect(item(page, 'imaging-app').filter({ hasText: 'Safety monitor' })).toContainText('Connected', { timeout: 15_000 });
  await imagingApp(page, 'Go silent');
  await expect(item(page, 'imaging-app').filter({ hasText: 'Safety monitor' })).toContainText('Gone quiet', { timeout: 60_000 });
});

test('no alert channel can send', async ({ page }) => {
  // inventory: no-channel
  await open(page);
  await expect(item(page, 'no-channel')).toHaveCount(0);
  await setConfig(page, { 'alerts.ntfy.enabled': false, 'alerts.mqtt.enabled': true, 'mqtt.enabled': false });
  await expect(item(page, 'no-channel')).toContainText('Alert channels');
  await expect(item(page, 'no-channel')).toContainText("Can't send");
});

test('a failed sensor keeps its card and is one row: name and pill', async ({ page }) => {
  // inventory: sensor-faults  inventory: cloud-card
  await open(page);
  await expect(card(page, 'Cloud Conditions')).not.toContainText('Error');
  await open(page, '?scenario=fail-ir');
  const row = statusCard(page).locator('.status-row[data-inventory="sensor-faults"]');
  await expect(row).toContainText('IR sky sensor', { timeout: 15_000 });
  await expect(row.locator('.pill')).toHaveText(/Error|Not responding|Stale/);
  await expect(row.getByRole('link')).toHaveAttribute('href', /tab=sensors/);
  // Said once (DS-08): the card keeps just its pill; what it affects is the row's "?".
  await expect(card(page, 'Cloud Conditions').locator('.pill')).toHaveText(/Error|Not responding|Stale/);
  for (const title of ['Cloud Conditions', 'IR Sky Sensor']) await expect(card(page, title)).not.toContainText("can't be measured");
});

test('settings not in effect are one row', async ({ page }) => {
  // inventory: settings-not-in-effect
  await open(page);
  await expect(item(page, 'settings-not-in-effect')).toContainText('1 inactive');
  await setConfig(page, { 'alerts.events.*.level': 2 });
  await expect(item(page, 'settings-not-in-effect')).toHaveCount(0);
});

test('a language that could not load is one row', async ({ page }) => {
  // inventory: language
  test.setTimeout(60_000);
  await open(page);
  await expect(item(page, 'language')).toHaveCount(0);
  await page.addInitScript(() => sessionStorage.setItem('sqm.demo.languageFail', '1'));
  await setConfig(page, { language: 'de' });
  await page.reload();
  await expect(item(page, 'language')).toContainText('Not loaded', { timeout: 15_000 });
});

test('no location: darkness unknown', async ({ page }) => {
  // inventory: clock-location
  await open(page);
  await expect(item(page, 'clock-location')).toHaveCount(0);
  await setConfig(page, { 'location.set': false, 'gps.enabled': false });
  await expect(item(page, 'clock-location')).toContainText('Location', { timeout: 10_000 });
  await expect(item(page, 'clock-location')).toContainText('Unknown');
});

test('safety rules not in effect and not shared with N.I.N.A.', async ({ page }) => {
  // inventory: rules-not-in-effect  inventory: safety-not-shared
  await open(page);
  await expect(item(page, 'rules-not-in-effect')).toHaveCount(0);
  await expect(item(page, 'safety-not-shared')).toHaveCount(0);
  await setConfig(page, { 'rain.enabled': false, 'alpaca.rainUnsafeEnabled': true, 'alpaca.enabled': false });
  // The rule is the row's label, the state its pill, the reason its "?" (DS-24).
  await expect(item(page, 'rules-not-in-effect').first()).toContainText('Unsafe while raining');
  await expect(item(page, 'rules-not-in-effect').first().locator('.pill')).toHaveText('Not in effect');
  await expect(item(page, 'safety-not-shared')).toContainText('Imaging apps');
  await expect(item(page, 'safety-not-shared').locator('.pill')).toHaveText('Not shared');
});

// DS-08 and DS-24 on what the dashboard renders, not just the English
// strings: copy composed in code can join facts too. Every card, in states
// that show the most text: no " · " or " - " run-ons, and the Status card's
// tiles carry no explanation under the pill.
for (const [state, query, patch] of [
  ['healthy', '', null],
  ['sensor fault', '?scenario=fail-ir', null],
  ['rain', '?scenario=rain', null],
  [
    'rules off and Alpaca off',
    '',
    { 'rain.enabled': false, 'alpaca.rainUnsafeEnabled': true, 'alpaca.enabled': false, 'alerts.enabled': false },
  ],
] as const) {
  test(`no run-on text on the dashboard: ${state}`, async ({ page }) => {
    await open(page, query);
    if (patch) await setConfig(page, patch);
    await page.waitForTimeout(2500);
    for (const text of await page.locator('.masonry-item').allInnerTexts()) {
      for (const line of text.split('\n')) {
        expect(line, 'DS-24: one fact per line').not.toMatch(/ [·•] | [-–] /);
      }
    }
    await expect(statusCard(page).locator('.status-tile a')).toHaveCount(0);
    for (const tile of await statusCard(page).locator('.status-tile:not([data-inventory="imaging-app"])').all()) {
      await expect(tile.locator('.metric-sub')).toHaveCount(0);
    }
  });
}

test('update available from this session’s check', async ({ page }) => {
  // inventory: update-available
  await open(page);
  await expect(item(page, 'update-available')).toHaveCount(0);
  await page.evaluate(() => sessionStorage.setItem('sqm.updates.latest', 'v9.9.9'));
  await page.reload();
  await expect(item(page, 'update-available')).toContainText('Update available: v9.9.9');
});

test('the demo says its clock moved on the Demo button, not the dashboard', async ({ page }) => {
  await open(page);
  await expect(page.locator('.demo-clock-moved')).toHaveCount(0);
  await open(page, '?scenario=night');
  await expect(page.locator('.demo-panel-toggle')).toContainText('Clock moved');
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

test('Device & Network: one address per row, local name and MQTT', async ({ page }) => {
  // inventory: ipv6  inventory: mdns  inventory: mqtt-state
  await open(page);
  const device = card(page, 'Device & Network');
  await expect(device.locator('.reading-row').filter({ hasText: 'IPv4' })).toBeVisible();
  await expect(item(page, 'ipv6')).toBeVisible();
  // Each IPv6 address is its own row (026 DS-05).
  const addresses = item(page, 'ipv6').locator('.reading-row');
  expect(await addresses.count()).toBeGreaterThan(0);
  for (const row of await addresses.all()) await expect(row.locator('.reading-value')).toHaveCount(1);
  await expect(item(page, 'mdns')).toContainText('.local');
  await expect(item(page, 'mqtt-state')).toContainText('Connected');
  await setConfig(page, { 'wifi.ipv6': false, 'wifi.mdns': false, 'mqtt.enabled': false });
  await expect(item(page, 'ipv6')).toHaveCount(0);
  await expect(item(page, 'mdns')).toHaveCount(0);
  await expect(item(page, 'mqtt-state')).toHaveCount(0);
});
