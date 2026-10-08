import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../Settings';
import Alpaca from '../Alpaca';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { mockDevice } from '../../test/mockDevice';
import type { DepFacts } from '../../lib/settingsDeps';

// Every catalogued dependency, in the UI (specs/020-settings-dependencies,
// FR-013b): the note with its reason, the one-click fix, and that a setting
// can't be switched on while what it needs is off but can always be
// switched off. The device side is test/test_settings_deps.

const alerts = mockConfig.alerts!;
const level = (key: keyof typeof alerts.events, value: number) => ({ ...alerts.events, [key]: { ...alerts.events[key], level: value } });
const mqttOn = { ...mockConfig.mqtt, enabled: true, broker: '192.168.1.10', topic: 'sqmeter' };
const mqttOff = { ...mockConfig.mqtt, enabled: false };

type Case = {
  id: string;
  tab: string;
  config?: Record<string, unknown>;
  facts?: Partial<DepFacts>;
  note: string; // exact note text
  control?: string; // checkbox or select label
  control_state?: 'locked' | 'unlocked';
  fixLabel?: string;
  fixTab?: string; // the tab the fix lands on
};

const CASES: Case[] = [
  { id: 'D-01', tab: 'alerts', config: { mqtt: mqttOff, alerts: { ...alerts, mqtt: { enabled: true } } }, note: 'Inactive - MQTT is off', control: 'MQTT', control_state: 'unlocked', fixLabel: 'Turn on MQTT', fixTab: 'Network' },
  { id: 'D-01', tab: 'alerts', config: { mqtt: mqttOff, alerts: { ...alerts, mqtt: { enabled: false } } }, note: 'MQTT is off.', control: 'MQTT', control_state: 'locked' },
  { id: 'D-02', tab: 'alerts', config: { mqtt: mqttOn, alerts: { ...alerts, mqtt: { enabled: true } } }, facts: { mqttConnected: false }, note: 'Inactive - Broker not connected', control: 'MQTT', control_state: 'unlocked' },
  { id: 'D-03', tab: 'alerts', config: { alerts: { ...alerts, ntfy: { ...alerts.ntfy, enabled: true } } }, facts: { wifiConnected: false }, note: 'Inactive - Not connected to WiFi', control: 'ntfy', control_state: 'unlocked', fixLabel: 'WiFi settings', fixTab: 'Network' },
  { id: 'D-05', tab: 'alerts', config: { rain: { ...mockConfig.rain, enabled: false }, alerts: { ...alerts, events: level('rain_started', 0) } }, note: 'Rain sensor is off.', control: 'Rain starts: level', control_state: 'locked', fixLabel: 'Turn on', fixTab: 'Sensors' },
  { id: 'D-05', tab: 'alerts', config: { rain: { ...mockConfig.rain, enabled: false }, alerts: { ...alerts, events: { ...level('rain_started', 2), rain_stopped: { level: 0, sound: '' } } } }, note: 'Inactive - Rain sensor is off', control: 'Rain starts: level', control_state: 'unlocked' },
  { id: 'D-06', tab: 'alerts', config: { alerts: { ...alerts, events: level('dew_risk', 2) } }, facts: { environmentDetected: false }, note: 'Inactive - BME280 not detected', control: 'Dew risk within: level', control_state: 'unlocked', fixLabel: 'Sky sensors', fixTab: 'Sensors' },
  { id: 'D-07', tab: 'alerts', config: { alerts: { ...alerts, events: { ...level('clouded_over', 2), clear_sky: { level: 0, sound: '' } } } }, facts: { infraredDetected: false }, note: 'Inactive - MLX90614 not detected', control: 'Skies cloud over above: level', control_state: 'unlocked' },
  { id: 'D-08', tab: 'alerts', config: { alerts: { ...alerts, events: level('unsafe', 4) } }, note: "Phones won't ring - Needs the Bluetooth firmware build", fixLabel: 'Bluetooth', fixTab: 'Device' },
  { id: 'D-09', tab: 'alerts', config: { location: { set: false, latitude: 0, longitude: 0 }, alerts: { ...alerts, skyNightOnly: true, safetyNightOnly: false } }, facts: { gpsFix: false }, note: 'Inactive - Needs your location', control: "Sky alerts only when it's dark", control_state: 'unlocked', fixLabel: 'Set location', fixTab: 'Time & Location' },
  { id: 'D-10', tab: 'alerts', config: { location: { set: false, latitude: 0, longitude: 0 }, alerts: { ...alerts, skyNightOnly: true, safetyNightOnly: false } }, facts: { gpsFix: false }, note: 'Needs your location.', control: "Safety alerts only when it's dark", control_state: 'locked' },
  { id: 'D-12', tab: 'alerts', config: { alpaca: { ...mockConfig.alpaca, enabled: false }, alerts: { ...alerts, armWithAlpaca: true } }, note: 'Inactive - Alpaca is off', control: 'On while N.I.N.A. is connected', control_state: 'unlocked', fixLabel: 'Turn on Alpaca', fixTab: 'Safety' },
  { id: 'D-13', tab: 'network', config: { mqtt: { ...mqttOn, homeAssistant: { enabled: true, discoveryPrefix: 'homeassistant' } }, alerts: { ...alerts, enabled: false } }, note: 'No alerts switch - Alerts are off', fixLabel: 'Turn on alerts', fixTab: 'Alerts' },
  { id: 'D-14', tab: 'network', config: { mqtt: { ...mqttOn, publish: { ...mqttOn.publish, rain: true } }, rain: { ...mockConfig.rain, enabled: false } }, note: 'Nothing to publish - Rain sensor is off', control: 'Rain', control_state: 'unlocked' },
  { id: 'D-15', tab: 'safety', config: { rain: { ...mockConfig.rain, enabled: false }, alpaca: { ...mockConfig.alpaca, rainUnsafeEnabled: true, rainSensorRequired: false } }, note: 'Not in effect - Rain sensor is off', control: 'Unsafe while raining', control_state: 'unlocked', fixLabel: 'Turn on', fixTab: 'Sensors' },
  { id: 'D-16', tab: 'safety', config: { wind: { ...mockConfig.wind, enabled: false }, alpaca: { ...mockConfig.alpaca, windSpeedUnsafeEnabled: true, windGustUnsafeEnabled: false } }, note: 'Reports unsafe - Anemometer is off', control: 'Max wind speed', control_state: 'unlocked' },
  { id: 'D-17', tab: 'safety', config: { alpaca: { ...mockConfig.alpaca, cloudCoverEnabled: true } }, facts: { infraredDetected: false }, note: 'Reports unsafe - MLX90614 not detected', control: 'Max cloud cover', control_state: 'unlocked' },
  { id: 'D-18', tab: 'safety', config: { alpaca: { ...mockConfig.alpaca, sqmMinEnabled: false } }, facts: { lightDetected: false }, note: 'TSL2591 not detected.', control: 'Min sky darkness', control_state: 'locked' },
  { id: 'D-19', tab: 'safety', config: { alpaca: { ...mockConfig.alpaca, humidityMaxEnabled: true, dewpointMarginEnabled: false } }, facts: { environmentDetected: false }, note: 'Reports unsafe - BME280 not detected', control: 'Max humidity', control_state: 'unlocked' },
  { id: 'D-24', tab: 'sensors', config: { rain: { ...mockConfig.rain, enabled: true, dailyResetEnabled: true } }, facts: { clockSet: false }, note: "Inactive - The device doesn't know the time yet", control: 'Reset the daily total', control_state: 'unlocked', fixLabel: 'Time sources', fixTab: 'Time & Location' },
  { id: 'D-25', tab: 'time', config: { location: { set: false, latitude: 0, longitude: 0, showSunMoon: true } }, facts: { gpsFix: false }, note: 'Inactive - Needs your location', control: 'Sun & Moon card on the dashboard', control_state: 'unlocked' },
  { id: 'D-26', tab: 'time', config: { gps: { ...mockConfig.gps, enabled: true } }, facts: { gpsFix: false }, note: 'Inactive - No GPS fix - using the location in Settings', control: 'GPS receiver', control_state: 'unlocked' },
  { id: 'D-28', tab: 'time', config: { ntp: { ...mockConfig.ntp, enabled: true } }, facts: { wifiConnected: false }, note: 'Inactive - Not connected to WiFi', fixLabel: 'WiFi settings', fixTab: 'Network' },
  { id: 'D-29', tab: 'sensors', config: { skyCalibration: { ...mockConfig.skyCalibration, enabled: false } }, facts: { lightDetected: false }, note: 'TSL2591 not detected.', control: 'Apply SQM offset', control_state: 'locked' },
  { id: 'D-32', tab: 'device', config: { ota: { enabled: true, password: '' } }, note: 'Inactive - Set an upload password', control: 'Command-line uploads (ArduinoOTA)', control_state: 'unlocked' },
  { id: 'D-35', tab: 'time', config: { gps: { ...mockConfig.gps, enabled: true } }, facts: { gpsRunning: false, gpsFix: false }, note: 'Inactive - GPS starts after a restart', control: 'GPS receiver', control_state: 'unlocked', fixLabel: 'Restart' },
  { id: 'D-36', tab: 'network', config: { wifi: { ...mockConfig.wifi, mdns: true } }, facts: { wifiConnected: false }, note: 'Inactive - Not connected to WiFi', control: 'Advertise on the network (mDNS)', control_state: 'unlocked' },
];

const control = (label: string) =>
  screen.queryByLabelText(label, { selector: 'input[type=checkbox]' }) ?? screen.getByLabelText(label, { selector: 'select' });

describe('settings dependencies in Settings (FR-013b)', () => {
  beforeAll(() => {
    Element.prototype.scrollIntoView = () => undefined; // fixes jump to their section
  });
  afterEach(() => window.history.replaceState(null, '', '/'));

  it.each(CASES.map((c) => [`${c.id}: ${c.note}`, c] as const))('%s', async (_name, c) => {
    let restarted = false;
    server.use(
      http.post('/api/restart', () => {
        restarted = true;
        return HttpResponse.json({ success: true });
      })
    );
    mockDevice({ config: c.config, facts: c.facts });
    window.history.replaceState(null, '', `/settings?tab=${c.tab}`);
    render(<Settings />);

    const note = (await screen.findAllByText(c.note, {}, { timeout: 3000 }))[0];
    if (c.control) {
      const input = control(c.control) as HTMLInputElement;
      if (c.control_state === 'locked') expect(input).toBeDisabled(); // can't be switched on
      else expect(input).not.toBeDisabled(); // already on: can always be switched off
    }
    if (c.fixLabel) {
      const row = note.closest('.note') as HTMLElement;
      fireEvent.click(within(row).getByRole('button', { name: c.fixLabel }));
      if (c.fixTab) expect(await screen.findByRole('tab', { name: c.fixTab, selected: true })).toBeInTheDocument();
      else await waitFor(() => expect(restarted).toBe(true));
    }
  });

  it('D-04: with alerts off the event levels are greyed out', async () => {
    mockDevice({ config: { alerts: { ...alerts, enabled: false } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByLabelText('It turns unsafe: level')).toBeDisabled();
  });

  it('D-11: "Dark means" is off while both night-only options are off', async () => {
    mockDevice({ config: { alerts: { ...alerts, skyNightOnly: false, safetyNightOnly: false } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByLabelText('Dark means')).toBeDisabled();
  });

  it('D-23: the wind vane only shows with the anemometer on', async () => {
    mockDevice({ config: { wind: { ...mockConfig.wind, enabled: false, directionEnabled: true } } });
    window.history.replaceState(null, '', '/settings?tab=sensors');
    render(<Settings />);
    await screen.findByLabelText('Anemometer');
    expect(screen.queryByLabelText('Wind vane')).toBeNull();
  });

  it('D-30/D-31: Bluetooth settings need the Bluetooth build, the phone alarm a passkey', async () => {
    mockDevice({ facts: { bluetoothBuild: false } });
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
    expect(await screen.findByText('Needs the Bluetooth firmware build, installed over USB.')).toBeInTheDocument();
    expect(screen.queryByLabelText('Turn on Bluetooth')).toBeNull();
  });

  it('FR-007: channels that can\'t deliver are not counted, and alerts reaching nowhere is a warning', async () => {
    mockDevice({ config: { mqtt: mqttOff, alerts: { ...alerts, pushover: { ...alerts.pushover, enabled: false }, ntfy: { ...alerts.ntfy, enabled: false }, webhook: { ...alerts.webhook, enabled: false }, mqtt: { enabled: true } } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByText('Alerts reach nowhere: no channel can deliver right now.')).toBeInTheDocument();
    expect(screen.getByText('No channels')).toBeInTheDocument();
    expect(screen.queryByText('Send test')).toBeNull(); // not offered on an inactive channel
  });

  it('FR-006: previews what saving would make inactive', async () => {
    mockDevice({ config: { mqtt: mqttOn, alerts: { ...alerts, mqtt: { enabled: true } } } });
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);
    fireEvent.click(await screen.findByLabelText('Publish to a broker'));
    expect(await screen.findByText(/Saving makes these inactive: .*MQTT alerts \(MQTT is off\)/)).toBeInTheDocument();
  });

  it('US1: an inactive setting keeps its value when saved (FR-001)', async () => {
    let saved: { alerts?: { mqtt?: { enabled?: boolean } }; mqtt?: { enabled?: boolean } } | null = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = (await request.json()) as typeof saved;
        return HttpResponse.json({ success: true });
      })
    );
    mockDevice({ config: { mqtt: mqttOn, alerts: { ...alerts, mqtt: { enabled: true } } } });
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);
    fireEvent.click(await screen.findByLabelText('Publish to a broker'));
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved!.mqtt?.enabled).toBe(false);
    expect(saved!.alerts?.mqtt?.enabled).toBe(true);
  });
});

describe('Alpaca page (D-20, D-21, D-22)', () => {
  it('D-20: says when Alpaca is off', async () => {
    mockDevice({ config: { alpaca: { ...mockConfig.alpaca, enabled: false } } });
    render(<Alpaca />);
    expect(await screen.findByText('Alpaca is off, so no devices are advertised.')).toBeInTheDocument();
  });

  it('D-21/D-22: shows only the properties the device reports (no rain or wind without the sensors)', async () => {
    server.use(
      http.get('/api/v1/observingconditions/0/devicestate', () =>
        HttpResponse.json({ Value: [{ Name: 'CloudCover', Value: 3 }, { Name: 'TimeStamp', Value: new Date().toISOString() }], ErrorNumber: 0, ErrorMessage: '', ClientTransactionID: 0, ServerTransactionID: 1 })
      )
    );
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: false }, wind: { ...mockConfig.wind, enabled: false } } });
    render(<Alpaca />);
    expect(await screen.findByText('CloudCover')).toBeInTheDocument();
    expect(screen.queryByText('RainRate')).toBeNull();
    expect(screen.queryByText('WindSpeed')).toBeNull();
  });
});
