import { test, expect, type BrowserContext, type Page, type Request } from '@playwright/test';

// Opt-in real notifications (specs/018 US2/US3). The services are answered
// by the test (context.route), so nothing really leaves CI.

const TOPIC = 'sqm-test-topic-42';
const USER_KEY = 'u'.repeat(30);
const APP_TOKEN = 'a'.repeat(30);
const CORS = { 'access-control-allow-origin': '*', 'access-control-allow-headers': '*', 'access-control-allow-methods': 'POST, OPTIONS' };

interface Seen {
  outside: string[];
  posts: Request[];
}

async function fakeServices(context: BrowserContext, pushoverStatus = 200): Promise<Seen> {
  const seen: Seen = { outside: [], posts: [] };
  await context.route(
    (url) => !['localhost', '127.0.0.1'].includes(url.hostname) && url.protocol.startsWith('http'),
    async (route) => {
      const request = route.request();
      seen.outside.push(new URL(request.url()).host);
      if (request.method() === 'OPTIONS') return route.fulfill({ status: 204, headers: CORS });
      seen.posts.push(request);
      const pushover = request.url().includes('pushover');
      const status = pushover ? pushoverStatus : 200;
      const body = status === 200 ? '{"status":1}' : JSON.stringify({ errors: ['user identifier is invalid'] });
      return route.fulfill({ status, headers: { ...CORS, 'content-type': 'application/json' }, body });
    },
  );
  return seen;
}

const openPanel = async (page: Page) => {
  await page.goto('./');
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2000);
  await page.getByRole('button', { name: /Demo/ }).click();
};

const turnOn = async (page: Page) => {
  await page.getByLabel('Send real notifications from this demo').check();
  await expect(page.getByText(/Your keys stay in this tab only/)).toBeVisible();
  await page.getByRole('button', { name: 'Turn on' }).click();
};

test('off by default: alerts are recorded and nothing is sent', async ({ page, context }) => {
  const seen = await fakeServices(context);
  await openPanel(page);
  await expect(page.getByText('Off: nothing leaves your browser.')).toBeVisible();
  await page.getByRole('button', { name: 'Rain', exact: true }).click();
  const test = await page.evaluate(async () => (await fetch('/api/alerts/test?channel=all', { method: 'POST' })).status);
  expect(test).toBe(202);
  const recent = await page.evaluate(async () => (await fetch('/api/alerts/recent')).json());
  expect(recent.alerts.length).toBeGreaterThan(0);
  await page.waitForTimeout(2000);
  expect(seen.outside).toEqual([]);
});

test('ntfy: the device’s request reaches ntfy.sh; the result shows; the rate limit applies', async ({ page, context }) => {
  const seen = await fakeServices(context);
  await openPanel(page);
  await turnOn(page);
  await page.getByLabel('ntfy.sh topic').fill(TOPIC);
  await page.getByRole('button', { name: 'Send a test' }).click();

  await expect(page.getByText(/ntfy: Delivered/)).toBeVisible();
  expect(seen.posts).toHaveLength(1);
  const sent = seen.posts[0];
  expect(sent.url()).toBe(`https://ntfy.sh/${TOPIC}`);
  const headers = sent.headers();
  expect(headers['title']).toMatch(/^SQMeter Demo: /);
  expect(headers['tags']).toBe('test_tube');
  expect(headers['priority']).toBeTruthy();

  const recent = await page.evaluate(async () => (await fetch('/api/alerts/recent')).json());
  expect(recent.alerts[0].channels.ntfy).toEqual({ status: 'sent', detail: 'Delivered' });

  await page.getByRole('button', { name: 'Send a test' }).click();
  await expect(page.getByText(/ntfy: Skipped: the demo sends at most one per channel every 30 s/)).toBeVisible();
  expect(seen.posts).toHaveLength(1);
  expect(new Set(seen.outside)).toEqual(new Set(['ntfy.sh']));
});

test('Pushover: the service’s error in plain words; keys never stored', async ({ page, context }) => {
  const seen = await fakeServices(context, 400);
  await openPanel(page);
  await turnOn(page);
  await page.getByLabel('Pushover user key').fill(USER_KEY);
  await page.getByLabel('Pushover app token').fill(APP_TOKEN);
  await page.getByRole('button', { name: 'Send a test' }).click();
  await expect(page.getByText(/Pushover: user identifier is invalid/)).toBeVisible();
  expect(seen.posts[0].url()).toBe('https://api.pushover.net/1/messages.json');
  expect(seen.posts[0].postData()).toContain(`user=${USER_KEY}`);

  const stored = await page.evaluate(() => JSON.stringify({ ...localStorage }) + JSON.stringify({ ...sessionStorage }) + location.href);
  expect(stored).not.toContain(USER_KEY);
  expect(stored).not.toContain(APP_TOKEN);
  await page.reload();
  await page.waitForTimeout(1500);
  const afterReload = await page.evaluate(() => JSON.stringify({ ...localStorage }) + JSON.stringify({ ...sessionStorage }));
  expect(afterReload).not.toContain(USER_KEY);
});

test('webhooks and MQTT without secure WebSockets are explained before trying', async ({ page }) => {
  await openPanel(page);
  await turnOn(page);
  await expect(page.getByText(/Webhooks and self-hosted ntfy servers need a real SQMeter/)).toBeVisible();
  await page.getByLabel('Broker (secure WebSocket)').fill('ws://broker.example:1883');
  await expect(page.getByText(/a wss:\/\/ address/)).toBeVisible();
});

test('MQTT over a secure WebSocket: connect, publish the device’s alert JSON to <base>/alerts', async ({ page, context }) => {
  const published: { topic: string; payload: string }[] = [];
  await context.routeWebSocket('wss://broker.example:8884/mqtt', (ws) => {
    ws.onMessage((message) => {
      const bytes = Buffer.from(message as Buffer);
      if (bytes[0] === 0x10) ws.send(Buffer.from([0x20, 2, 0, 0])); // CONNACK, accepted
      if ((bytes[0] & 0xf0) === 0x30) {
        // PUBLISH QoS 0; remaining length is two bytes for a message this size
        const start = bytes[1] & 0x80 ? 3 : 2;
        const topicLength = (bytes[start] << 8) | bytes[start + 1];
        const topic = bytes.subarray(start + 2, start + 2 + topicLength).toString();
        published.push({ topic, payload: bytes.subarray(start + 2 + topicLength).toString() });
      }
    });
  });
  await openPanel(page);
  await turnOn(page);
  await page.getByLabel('Broker (secure WebSocket)').fill('wss://broker.example:8884/mqtt');
  await page.getByLabel('Base topic').fill('observatory');
  await page.getByRole('button', { name: 'Send a test' }).click();
  await expect(page.getByText(/MQTT: Published/)).toBeVisible();
  await expect.poll(() => published.length).toBe(1);
  expect(published[0].topic).toBe('observatory/alerts');
  expect(JSON.parse(published[0].payload)).toMatchObject({ event: 'test', device: 'SQMeter Demo' });
});
