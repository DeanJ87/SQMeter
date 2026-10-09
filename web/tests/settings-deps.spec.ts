import { test, expect, type Page } from '@playwright/test';

// Settings dependencies in the demo, which runs the device's own rules
// (specs/020-settings-dependencies US3, SC-001): the device's report, the
// Settings page and alert delivery agree.

const api = (page: Page, path: string, init?: RequestInit) =>
  page.evaluate(
    async ([p, i]) => {
      const response = await fetch(p as string, i as RequestInit | undefined);
      return { status: response.status, body: await response.text() };
    },
    [path, init] as const,
  );

const post = (page: Page, path: string, body: unknown) =>
  api(page, path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });

const entry = async (page: Page, setting: string) => {
  const report = JSON.parse((await api(page, '/api/settings/effective')).body);
  return report.settings.find((e: { setting: string }) => e.setting === setting);
};

test('MQTT alerts with MQTT off: reported inactive, shown inactive, skipped not failed', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2500);

  // Set up through the API only (US3 independent test).
  expect((await post(page, '/api/config', { alerts: { enabled: true, mqtt: { enabled: true } }, mqtt: { enabled: false } })).status).toBe(
    200,
  );
  expect(await entry(page, 'alerts.mqtt.enabled')).toMatchObject({
    id: 'D-01',
    state: 'inactive',
    reason: 'mqtt-off',
    text: 'MQTT is off',
    fix: 'network#mqtt',
  });

  await page.goto('./#/settings?tab=alerts');
  await expect(page.getByText('Inactive: MQTT is off')).toBeVisible();
  await page.getByRole('button', { name: 'Turn on MQTT' }).click();
  await expect(page.getByRole('tab', { name: 'Network', selected: true })).toBeVisible();

  // A test to every channel: MQTT is skipped with the reason, never "failed".
  expect((await api(page, '/api/alerts/test?channel=all', { method: 'POST' })).status).toBe(202);
  const recent = JSON.parse((await api(page, '/api/alerts/recent')).body);
  expect(recent.alerts[0].channels.mqtt).toEqual({ status: 'skipped', detail: 'MQTT is off' });

  // Switching MQTT back on restores it, unchanged (US2).
  await post(page, '/api/config', { mqtt: { enabled: true, broker: '192.168.1.10', topic: 'sqmeter' } });
  expect(await entry(page, 'alerts.mqtt.enabled')).toMatchObject({ id: 'D-01', state: 'active' });
  expect(JSON.parse((await api(page, '/api/config')).body).alerts.mqtt.enabled).toBe(true);
});

test('rain rules without the rain sensor are listed as not in effect (D-15)', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('main')).toBeVisible();
  await post(page, '/api/config', { rain: { enabled: false }, alpaca: { rainUnsafeEnabled: true } });
  await page.waitForTimeout(1500);
  const safety = JSON.parse((await api(page, '/api/safety')).body);
  expect(safety.rulesNotInEffect).toContain('Unsafe while raining - rain sensor is off');
  expect(await entry(page, 'alpaca.rainUnsafeEnabled')).toMatchObject({ state: 'inactive', unmet: 'inactive', reason: 'rain-off' });
});
