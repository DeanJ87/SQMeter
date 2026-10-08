import { http, HttpResponse, ws } from 'msw';
import type { AlertScheduleReason } from '../types';
import {
  generateSensorData,
  mockStatus,
  mockConfig,
  mockWifiNetworks,
  mockGithubReleases,
  mockAlpacaDevices,
  mockRecentAlerts,
} from './data';

// Demo-only: alerts switched on/off.
const mockAlertsArmed: { value: boolean; reason: AlertScheduleReason; since: string | null } = { value: true, reason: 'none', since: null };
const mockSchedule = () => ({
  armed: mockAlertsArmed.value,
  armWithAlpaca: false,
  mode: 'any' as const,
  reason: mockAlertsArmed.reason as AlertScheduleReason,
  since: mockAlertsArmed.since,
  sinceAgeMs: mockAlertsArmed.since ? Date.now() - Date.parse(mockAlertsArmed.since) : null,
});
const setMockArmed = (request: Request, armed: boolean) => {
  mockAlertsArmed.value = armed;
  mockAlertsArmed.reason = new URL(request.url).searchParams.get('source') === 'ui' ? 'user-ui' : 'user-rest';
  mockAlertsArmed.since = new Date().toISOString();
};

const alpacaEnvelope = <T>(Value: T) => ({
  Value,
  ClientTransactionID: 0,
  ServerTransactionID: 1,
  ErrorNumber: 0,
  ErrorMessage: '',
});

// WebSocket handlers — wildcard host works on both localhost and GitHub Pages
// The demo "booted" when the page loaded, mockStatus.uptime seconds ago.
const demoStartedAt = Date.now();
const demoUptime = () => mockStatus.uptime + Math.floor((Date.now() - demoStartedAt) / 1000);

const sensorSocket = ws.link('*/ws/sensors');
const statusSocket = ws.link('*/ws/status');

export const handlers = [
  // REST — sensors snapshot
  http.get('/api/sensors', () => HttpResponse.json(generateSensorData())),

  // REST — system status (refresh uptime each call)
  http.get('/api/status', () =>
    HttpResponse.json({
      ...mockStatus,
      alerts: mockSchedule(),
      uptime: demoUptime(),
      time: { iso: new Date().toISOString(), timezone: 'GMT0' },
    }),
  ),

  // ASCOM Alpaca — management + device state
  http.get('/management/v1/configureddevices', () => HttpResponse.json(alpacaEnvelope(mockAlpacaDevices))),
  http.get('/management/v1/description', () =>
    HttpResponse.json(alpacaEnvelope({ ServerName: 'SQMeter', Manufacturer: 'SQMeter', ManufacturerVersion: 'demo', Location: 'Demo' })),
  ),
  http.get('/api/v1/safetymonitor/0/devicestate', () =>
    HttpResponse.json(
      alpacaEnvelope([
        { Name: 'IsSafe', Value: true },
        { Name: 'TimeStamp', Value: new Date().toISOString() },
      ]),
    ),
  ),
  http.get('/api/v1/observingconditions/0/devicestate', () => {
    const data = generateSensorData();
    return HttpResponse.json(
      alpacaEnvelope([
        { Name: 'CloudCover', Value: data.clouds.coverPercent ?? 0 },
        { Name: 'DewPoint', Value: data.environment?.dewpoint ?? 0 },
        { Name: 'Humidity', Value: data.environment?.humidity ?? 0 },
        { Name: 'SkyQuality', Value: data.sky.sqm ?? 0 },
        { Name: 'Temperature', Value: data.environment?.temperature ?? 0 },
        { Name: 'WindDirection', Value: data.wind?.direction ?? 0 },
        { Name: 'WindGust', Value: data.wind?.gust ?? 0 },
        { Name: 'WindSpeed', Value: data.wind?.speed ?? 0 },
        { Name: 'TimeStamp', Value: new Date().toISOString() },
      ]),
    );
  }),

  // REST — SafetyMonitor verdict
  http.get('/api/safety/history', () => {
    const now = Math.floor(Date.now() / 1000);
    return HttpResponse.json({
      boot: 3,
      uptime: 4000,
      entries: [
        { kind: 'alert', boot: 3, uptime: 3900, timestamp: now - 100, safe: false },
        { kind: 'change', boot: 3, uptime: 3899, timestamp: now - 101, safe: false, held: false, reasonFlags: 0x30 },
        { kind: 'change', boot: 3, uptime: 181, timestamp: now - 3819, safe: true },
        { kind: 'change', boot: 3, uptime: 1, safe: false, held: true, reasonFlags: 0 },
        { kind: 'boot', boot: 3, uptime: 0, resetReason: 3 },
        { kind: 'alert', boot: 2, uptime: 900, timestamp: now - 5000, safe: false },
      ],
    });
  }),
  http.get('/api/safety', () => HttpResponse.json(generateSensorData().safety)),

  // REST — alerts
  http.get('/api/alerts/recent', () => HttpResponse.json({ enabled: true, armed: mockAlertsArmed.value, alerts: mockRecentAlerts })),
  http.get('/api/alerts/armed', () => HttpResponse.json(mockSchedule())),
  http.post('/api/alerts/arm', ({ request }) => {
    setMockArmed(request, true);
    return HttpResponse.json({ success: true, armed: true }, { status: 202 });
  }),
  http.post('/api/alerts/disarm', ({ request }) => {
    setMockArmed(request, false);
    return HttpResponse.json({ success: true, armed: false }, { status: 202 });
  }),
  http.post('/api/alerts/clear', () => {
    mockRecentAlerts.splice(0, mockRecentAlerts.length);
    return HttpResponse.json({ success: true });
  }),
  http.post('/api/alerts/test', () => HttpResponse.json({ success: true, message: 'Test notification queued' }, { status: 202 })),

  // REST — config
  http.get('/api/config', () => HttpResponse.json(mockConfig)),
  http.post('/api/config', () => HttpResponse.json({ success: true })),
  http.put('/api/config', () => HttpResponse.json({ success: true })),

  // REST — wifi (returns { networks: [...] } to match firmware API shape)
  http.get('/api/wifi/scan', () => HttpResponse.json({ networks: mockWifiNetworks })),
  http.post('/api/wifi/connect', () => HttpResponse.json({ success: true, pending: true, message: 'Connection started' }, { status: 202 })),

  // REST — dark calibration
  http.post('/api/sensors/tsl2591/calibrate-dark', () =>
    HttpResponse.json({ success: true, darkVisibleOffset: 2.4, sampleCount: 150, darkCalibratedAt: Math.floor(Date.now() / 1000) }),
  ),

  // REST — MQTT test
  http.post('/api/mqtt/test', () => HttpResponse.json({ success: true, message: 'Connection successful (demo)' })),

  // REST — RG-15 communication test
  http.post('/api/sensors/rg15/test', () =>
    HttpResponse.json({
      ok: true,
      command: 'R',
      bytes_written: 2,
      elapsed_ms: 42,
      raw_response: 'Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h',
      ack: 'm',
      acknowledged: true,
      parsed: true,
      online: true,
      stale: false,
      error: null,
      hint: 'Check RG-15 Serial OUT -> ESP32 RX, Serial IN -> ESP32 TX, common ground, baud rate, and voltage level.',
    }),
  ),
  http.post('/api/sensors/rg15/reset-total', () =>
    HttpResponse.json({ ok: true, command: 'O', message: 'RG-15 total accumulation reset command sent' }),
  ),
  http.post('/api/sensors/rg15/reboot', () => HttpResponse.json({ ok: true, command: 'K', message: 'RG-15 reboot command sent' })),

  // REST — control
  http.post('/api/restart', () => HttpResponse.json({ ok: true })),
  http.post('/api/update', () => HttpResponse.json({ success: true })),
  http.post('/api/update/fs', () => HttpResponse.json({ success: true })),

  // REST — GitHub release updates
  http.get('/api/updates/check', ({ request }) => {
    const track = new URL(request.url).searchParams.get('track') === 'beta' ? 'beta' : 'stable';
    return HttpResponse.json(mockGithubReleases.filter((r) => r.prerelease === (track === 'beta')));
  }),
  http.post('/api/updates/apply', () => HttpResponse.json({ success: true, message: 'Update started' })),

  // WebSocket — push sensor data every second
  sensorSocket.addEventListener('connection', ({ client }) => {
    let lastSensorData = generateSensorData();
    client.send(JSON.stringify(lastSensorData));

    const interval = setInterval(() => {
      if (Date.now() - lastSensorData.timestamp * 1000 >= mockConfig.sensor.readIntervalMs) {
        lastSensorData = generateSensorData();
      }
      client.send(JSON.stringify(lastSensorData));
    }, 1000);

    client.addEventListener('close', () => clearInterval(interval));
  }),

  // WebSocket — push system status every 2 seconds, like the device
  statusSocket.addEventListener('connection', ({ client }) => {
    const send = () =>
      client.send(
        JSON.stringify({
          ...mockStatus,
          uptime: demoUptime(),
          time: { iso: new Date().toISOString(), timezone: 'GMT0' },
        }),
      );

    send();
    const interval = setInterval(send, 2000);
    client.addEventListener('close', () => clearInterval(interval));
  }),
];
