import { http, HttpResponse, ws } from 'msw';
import { demoDevice, type Reply } from './device';
import { mockGithubReleases, mockStatus, mockWifiNetworks } from '../mocks/data';

// The demo's requests, answered by the emulated device (./device.ts) the way
// the firmware answers them. Anything a real device would send somewhere
// (update checks, alerts, MQTT, WiFi) is simulated and marked `demo: true`;
// nothing leaves the browser (specs/016-demo-device-emulation, FR-006).

const json = (body: string, status = 200) => new HttpResponse(body, { status, headers: { 'Content-Type': 'application/json' } });

const reply = (r: Reply) =>
  new HttpResponse(r.body, { status: r.status, headers: { 'Content-Type': r.contentType ?? 'application/json' } });

// Who paused or resumed alerts: the web UI adds ?source=ui, scripts don't.
const armSource = (request: Request) => (new URL(request.url).searchParams.get('source') === 'ui' ? 'ui' : 'rest');

const queryParams = (request: Request) => Object.fromEntries(new URL(request.url).searchParams.entries());

// Alpaca parameters: query string for GET, form body for PUT.
async function alpacaParams(request: Request): Promise<[string, string][]> {
  const params = [...new URL(request.url).searchParams.entries()];
  if (request.method === 'PUT') {
    const text = await request.text();
    params.push(...new URLSearchParams(text).entries());
  }
  return params;
}

// GET /api/status: the device core's decisions (sky, sensors, diagnostics,
// uptime) with the hardware sections a real ESP32 would report.
export function statusDocument() {
  const parts = demoDevice.statusParts();
  const cfg = demoDevice.rawConfig();
  return {
    ...mockStatus,
    ...parts,
    firmware: { ...mockStatus.firmware, version: '0.2.0-beta.3' },
    time: { iso: demoDevice.isoTime, timezone: cfg.ntp?.timezone ?? 'UTC0' },
    wifi: {
      ...mockStatus.wifi,
      ssid: joinedSsid ?? (cfg.wifi?.ssid || mockStatus.wifi.ssid),
      hostname: cfg.wifi?.hostname,
      mdns: cfg.wifi?.mdns ?? true,
      apMode: false,
      connectPending: false,
    },
    mqtt: cfg.mqtt?.enabled
      ? {
          ...mockStatus.mqtt,
          enabled: true,
          connected: true,
          broker: cfg.mqtt.broker,
          port: cfg.mqtt.port,
          topic: cfg.mqtt.topic,
          availabilityTopic: `${cfg.mqtt.topic}/availability`,
        }
      : { ...mockStatus.mqtt, enabled: false, connected: false, availabilityTopic: `${cfg.mqtt?.topic ?? 'sqmeter'}/availability` },
  };
}

// The network the WiFi setup screen "joined" (simulated).
let joinedSsid: string | null = null;

const simulated = (extra: Record<string, unknown> = {}) => HttpResponse.json({ success: true, demo: true, ...extra });

export const demoHandlers = [
  // Readings and status
  http.get('/api/sensors', () => json(demoDevice.readings())),
  http.get('/api/status', () => HttpResponse.json(statusDocument())),
  http.get(
    '/api/safe',
    () => new HttpResponse(JSON.parse(demoDevice.safety()).safe ? '1' : '0', { headers: { 'Content-Type': 'text/plain' } }),
  ),
  http.get('/api/safety/history', () => json(demoDevice.safetyHistory())),
  http.get('/api/safety', () => json(demoDevice.safety())),

  // Settings
  http.get('/api/config', () => json(demoDevice.getConfig())),
  http.post('/api/config', async ({ request }) => reply(demoDevice.applyConfig(await request.text()))),
  http.put('/api/config', async ({ request }) => reply(demoDevice.applyConfig(await request.text()))),
  http.post('/api/restart', () => {
    demoDevice.restart();
    return HttpResponse.json({ success: true, message: 'Restarting...' });
  }),

  // Alerts
  http.get('/api/alerts/recent', () => json(demoDevice.recentAlerts())),
  http.get('/api/alerts/armed', () => json(demoDevice.armedDocument())),
  http.post('/api/alerts/arm', ({ request }) => {
    demoDevice.setArmed(true, armSource(request));
    return HttpResponse.json({ success: true, armed: true }, { status: 202 });
  }),
  http.post('/api/alerts/disarm', ({ request }) => {
    demoDevice.setArmed(false, armSource(request));
    return HttpResponse.json({ success: true, armed: false }, { status: 202 });
  }),
  http.post('/api/alerts/clear', () => {
    demoDevice.clearAlerts();
    return HttpResponse.json({ success: true });
  }),
  http.post('/api/alerts/test', ({ request }) => reply(demoDevice.testAlert(queryParams(request)))),
  http.post('/api/ble/ack', () => HttpResponse.json({ error: 'No phone alarm is active' }, { status: 409 })),
  http.post('/api/ble/forget-bonds', () => HttpResponse.json({ error: 'Bluetooth is off' }, { status: 409 })),

  // Sensors
  http.post('/api/sensors/tsl2591/calibrate-dark', () => reply(demoDevice.calibrateDark())),
  http.post('/api/sensors/rg15/test', () => {
    if (!demoDevice.rawConfig().rain?.enabled)
      return HttpResponse.json({ error: 'The rain sensor is switched off (Settings → Sensors → Rain sensor)' }, { status: 409 });
    const rain = demoDevice.statusParts().diagnostics?.rain ?? {};
    return HttpResponse.json({
      success: true,
      command: 'R',
      bytesWritten: 2,
      elapsedMs: 42,
      rawResponse: rain.lastResponse ?? '',
      ack: true,
      online: true,
      lastSuccessfulReadAgeMs: 0,
      demo: true,
    });
  }),
  http.post('/api/sensors/rg15/reset-total', () => simulated({ command: 'O', message: 'RG-15 total accumulation reset command sent' })),
  http.post('/api/sensors/rg15/reboot', () => simulated({ command: 'K', message: 'RG-15 reboot command sent' })),

  // Network (simulated: nothing is contacted)
  http.get('/api/wifi/scan', () => HttpResponse.json({ success: true, scanning: false, networks: mockWifiNetworks })),
  http.post('/api/wifi/connect', async ({ request }) => {
    const body = (await request.json().catch(() => ({}))) as { ssid?: string; password?: string };
    if (typeof body.ssid !== 'string' || typeof body.password !== 'string')
      return HttpResponse.json({ error: 'Missing SSID or password' }, { status: 400 });
    joinedSsid = body.ssid; // nothing is joined: the demo just says it was
    return HttpResponse.json({ success: true, pending: true, message: 'Connection started', demo: true }, { status: 202 });
  }),
  http.post('/api/mqtt/test', () => simulated({ message: 'Demo: nothing was sent - a real SQMeter would connect to your broker here' })),

  // Updates (simulated: no download, nothing flashed)
  http.get('/api/updates/check', ({ request }) => {
    const track = new URL(request.url).searchParams.get('track') === 'beta' ? 'beta' : 'stable';
    return HttpResponse.json(mockGithubReleases.filter((r) => r.prerelease === (track === 'beta')));
  }),
  http.post('/api/updates/apply', () => simulated({ message: 'Update started' })),
  http.post('/api/update', () => simulated()),
  http.post('/api/update/fs', () => simulated()),

  // ASCOM Alpaca: the firmware's own router, in the device core
  http.get('/management/*', async ({ request }) =>
    reply(demoDevice.alpaca('GET', new URL(request.url).pathname, await alpacaParams(request))),
  ),
  http.get('/api/v1/*', async ({ request }) => reply(demoDevice.alpaca('GET', new URL(request.url).pathname, await alpacaParams(request)))),
  http.put('/api/v1/*', async ({ request }) => reply(demoDevice.alpaca('PUT', new URL(request.url).pathname, await alpacaParams(request)))),

  // Live updates, at the device's rates
  ws.link('*/ws/sensors').addEventListener('connection', ({ client }) => {
    const send = () => client.send(demoDevice.readings());
    send();
    const unsubscribe = demoDevice.onChange(send);
    client.addEventListener('close', () => unsubscribe());
  }),
  ws.link('*/ws/status').addEventListener('connection', ({ client }) => {
    const send = () => client.send(JSON.stringify(statusDocument()));
    send();
    const interval = setInterval(send, 2000);
    client.addEventListener('close', () => clearInterval(interval));
  }),
];
