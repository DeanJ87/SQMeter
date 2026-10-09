import { describe, expect, it } from 'vitest';
import { generateSensorData, mockConfig, mockStatus } from '../../mocks/data';
import type { AlertSchedule, Config, SensorData, SystemStatus } from '../../types';
import type { EffectiveReport } from '../../lib/settingsDeps';
import { expectedSensors, glanceItems, toCheck, type GlanceInput } from '../glance';

// Every visibility rule of the Status card (specs/025 FR-005..FR-017, 026 FR-002).

const healthySensors = (): SensorData => {
  const s = generateSensorData();
  return {
    ...s,
    timeValid: true,
    dataStale: false,
    safety: { ...s.safety!, safe: true, rawSafe: true, reasons: [], secondsUntilSafe: 0 },
  };
};

const input = (over: Partial<GlanceInput> = {}): GlanceInput => ({
  sensors: healthySensors(),
  status: structuredClone(mockStatus),
  config: structuredClone(mockConfig),
  effective: { facts: {} as EffectiveReport['facts'], settings: [] },
  connected: true,
  quiet: false,
  schedule: { armed: true, mode: 'any', reason: 'none', since: null, sinceAgeMs: null },
  ...over,
});

const ids = (over: Partial<GlanceInput>) =>
  glanceItems(input(over))
    .filter((item) => item.severity !== 'ok')
    .map((item) => item.id);

const withConfig = (change: (config: Config) => void) => {
  const config = structuredClone(mockConfig);
  change(config);
  return config;
};

const withStatus = (change: (status: SystemStatus) => void) => {
  const status = structuredClone(mockStatus);
  change(status);
  return status;
};

describe('glanceItems', () => {
  it('is all good when all is well', () => {
    const items = glanceItems(input({ config: withConfig((c) => (c.alerts!.enabled = true)) }));
    expect(toCheck(items)).toEqual([]);
    // Tiles: Data, Safety, Alerts; the mock device has an imaging app connected - a tile, never a problem.
    expect(items.map((item) => [item.label, item.state])).toEqual(
      expect.arrayContaining([
        ['Data', 'Live'],
        ['Safety', 'Safe'],
        ['Alerts', 'Sending'],
        ['Imaging app', 'Connected'],
      ]),
    );
  });

  it('says when the stream is down, quiet or the data is stale (FR-014)', () => {
    expect(glanceItems(input({ connected: false }))[0]).toMatchObject({
      id: 'freshness',
      severity: 'problem',
      state: 'Offline',
    });
    expect(glanceItems(input({ quiet: true }))[0].state).toBe('No updates');
    expect(glanceItems(input({ sensors: { ...healthySensors(), dataStale: true, dataAgeMs: 40_000 } }))[0]).toMatchObject({
      state: 'Stale',
      sub: '40.0 s old',
    });
  });

  it('puts an unsafe verdict first after freshness, with its reasons', () => {
    const sensors = healthySensors();
    sensors.safety = { ...sensors.safety!, safe: false, rawSafe: false, reasons: ['Rain detected'] };
    const items = glanceItems(input({ sensors }));
    expect(items[1]).toMatchObject({ id: 'safety-verdict', severity: 'problem', state: 'Unsafe', sub: 'Rain detected' });
  });

  it('shows paused alerts with a Resume action (FR-008)', () => {
    const schedule: AlertSchedule = { armed: false, mode: 'any', reason: 'user-ui', since: null, sinceAgeMs: 60_000 };
    const item = glanceItems(input({ schedule, config: withConfig((c) => (c.alerts!.enabled = true)) })).find(
      (i) => i.id === 'alerts-state',
    );
    expect(item).toMatchObject({ severity: 'problem', state: 'Paused', fix: { action: 'resume' } });
    expect(item?.sub).toMatch(/^Paused by you/);
  });

  it('says alerts are waiting for an imaging app (US1-3)', () => {
    const schedule: AlertSchedule = { armed: false, mode: 'whileConnected', reason: 'waiting-for-client', since: null, sinceAgeMs: null };
    const config = withConfig((c) => {
      c.alerts!.enabled = true;
      c.alerts!.sendMode = 'whileConnected';
    });
    const item = glanceItems(input({ schedule, config })).find((i) => i.id === 'alerts-state');
    expect(item).toMatchObject({ state: 'Waiting', detail: 'Nothing is sent until an imaging app connects.' });
    expect(item?.sub).toMatch(/^Waiting for an imaging app/);
  });

  it('says alerts are off once', () => {
    expect(ids({ config: withConfig((c) => (c.alerts!.enabled = false)) })).toContain('alerts-state');
  });

  it('says the send mode is not in effect when Alpaca is off (FR-010)', () => {
    const config = withConfig((c) => {
      c.alerts!.enabled = true;
      c.alerts!.sendMode = 'whileConnected';
    });
    const status = withStatus((s) => (s.alpaca!.enabled = false));
    expect(ids({ config, status })).toContain('alerts-mode-not-in-effect');
  });

  it('says no channel can send (US1-6)', () => {
    const config = withConfig((c) => {
      c.alerts!.enabled = true;
      c.alerts!.pushover.enabled = false;
      c.alerts!.ntfy.enabled = false;
      c.alerts!.webhook.enabled = false;
      c.alerts!.mqtt.enabled = true;
    });
    const effective: EffectiveReport = {
      facts: {} as EffectiveReport['facts'],
      settings: [{ id: 'D-01', setting: 'alerts.mqtt.enabled', state: 'inactive', reason: 'mqtt-off', text: 'MQTT is off' }],
    };
    const item = glanceItems(input({ config, effective })).find((i) => i.id === 'no-channel');
    expect(item).toMatchObject({ severity: 'problem', detail: 'MQTT is off' });
    const none = withConfig((c) => {
      c.alerts!.enabled = true;
      (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).forEach((channel) => (c.alerts![channel].enabled = false));
    });
    expect(ids({ config: none })).toContain('no-channel');
  });

  it('shows the imaging app when it matters, and a silent one as a problem (FR-009)', () => {
    const quiet = { connected: false, watching: false, silent: false, lastCheckedAgeMs: null, clientId: null };
    const status = withStatus((s) => {
      s.alpaca!.clients = { safetymonitor: { ...quiet }, observingconditions: { ...quiet } };
    });
    expect(glanceItems(input({ status })).some((i) => i.id === 'imaging-app')).toBe(false);
    const silent = withStatus((s) => {
      s.alpaca!.clients = {
        safetymonitor: { ...quiet, connected: true, silent: true, lastCheckedAgeMs: 120_000 },
        observingconditions: { ...quiet },
      };
    });
    expect(glanceItems(input({ status: silent })).find((i) => i.id === 'imaging-app')).toMatchObject({ severity: 'problem' });
    const needed = withConfig((c) => {
      c.alerts!.enabled = true;
      c.alerts!.sendMode = 'whileConnected';
    });
    expect(glanceItems(input({ status, config: needed })).filter((i) => i.id === 'imaging-app')).toHaveLength(2);
  });

  it('keeps a failed sensor visible but not an unfitted one (FR-013, D6)', () => {
    const missingBme = withStatus((s) => (s.sensors.environment = { status: 'missing' }));
    expect(expectedSensors(missingBme, mockConfig)).not.toContain('environment');
    const failedBme = withStatus((s) => (s.sensors.environment = { status: 'error', ageMs: 90_000 }));
    expect(ids({ status: failedBme })).toContain('sensor-faults');
    const deadIr = withStatus((s) => (s.sensors.infrared = { status: 'missing' }));
    expect(glanceItems(input({ status: deadIr })).find((i) => i.id === 'sensor-faults')).toMatchObject({
      label: 'IR sky sensor',
      state: 'Not responding',
    });
  });

  it('summarises settings not in effect that matter (FR-011)', () => {
    const effective: EffectiveReport = {
      facts: {} as EffectiveReport['facts'],
      settings: [
        { id: 'D-05', setting: 'alerts.events.rain_started.level', state: 'inactive', text: 'Rain sensor is off' },
        { id: 'D-14', setting: 'mqtt.publish.gps', state: 'inactive', text: 'GPS is off' },
      ],
    };
    expect(glanceItems(input({ effective })).find((i) => i.id === 'settings-not-in-effect')).toMatchObject({
      label: 'Settings',
      state: '1 not in effect',
      detail: 'Rain sensor is off',
    });
  });

  it('says the clock or location is unknown (FR-016)', () => {
    expect(ids({ sensors: { ...healthySensors(), timeValid: false } })).toContain('clock-location');
    const noLocation = withStatus((s) => (s.sky = { locationSource: 'none', nightKnown: false }));
    expect(glanceItems(input({ status: noLocation })).find((i) => i.id === 'clock-location')).toMatchObject({
      label: 'Location',
      state: 'Unknown',
    });
  });

  it('shows a ringing phone alarm with Acknowledge (FR-017)', () => {
    const status = withStatus((s) => {
      s.ble = {
        available: true,
        active: true,
        clients: 1,
        alarm: { serviceActive: true, active: true, sequence: 3, acknowledgedSequence: 2, bondedPhones: 1 },
      };
    });
    expect(glanceItems(input({ status })).find((i) => i.id === 'phone-alarm')?.fix).toMatchObject({ action: 'acknowledge' });
  });

  it('orders problems by the spec priority', () => {
    const status = withStatus((s) => (s.sensors.infrared = { status: 'missing' }));
    const sensors = { ...healthySensors(), timeValid: false };
    const order = glanceItems(input({ connected: false, status, sensors })).map((item) => item.id);
    expect(order.indexOf('freshness')).toBeLessThan(order.indexOf('sensor-faults'));
    expect(order.indexOf('sensor-faults')).toBeLessThan(order.indexOf('clock-location'));
  });
});
