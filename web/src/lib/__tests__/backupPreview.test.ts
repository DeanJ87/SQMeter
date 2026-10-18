import { describe, expect, it } from 'vitest';
import {
  changedSettings,
  changesNothing,
  missingPaths,
  previewDetails,
  restoreWarnings,
  sectionOf,
  type RestorePreview,
  type Settings,
} from '../backupPreview';

// What a restore preview lists (specs/033-config-backup FR-007, FR-008).

const current: Settings = {
  deviceName: 'backyard-sqm',
  wifi: { ssid: 'Observatory', password: '********', hostname: 'sqmeter', mdns: true },
  mqtt: { broker: 'broker.lan', port: 1883, password: '', publish: { sky: true, diagnostics: false } },
  auth: { enabled: false, username: 'admin', password: '' },
  alerts: { sendMode: 'any', armWithAlpaca: false },
  wind: { enabled: false, speedPin: 27 },
};

const preview = (settings: Settings, extra: Partial<RestorePreview> = {}): RestorePreview => ({
  token: 'aa',
  backup: { format: 1, firmware: '0.3.4-beta.1' },
  settings,
  secrets: { state: 'none', changed: [], unchanged: [] },
  notApplicable: [],
  restart: [],
  ...extra,
});

const facts = { firmware: '0.3.4-beta.1', imagingAppConnected: false };
const copy = (): Settings => JSON.parse(JSON.stringify(current)) as Settings;

describe('restore preview details', () => {
  it('lists nothing for an unchanged backup', () => {
    const details = previewDetails(current, copy(), preview(copy()), facts);
    expect(details).toEqual({ changes: [], kept: [], ignored: [], warnings: [] });
    expect(changesNothing(details, preview(copy()))).toBe(true);
  });

  it('lists exactly the settings that change, never secrets or the derived armWithAlpaca', () => {
    const after = copy();
    (after.mqtt as Settings).broker = '192.168.1.10';
    (after.wifi as Settings).password = 'changed';
    (after.alerts as Settings).sendMode = 'whileConnected';
    (after.alerts as Settings).armWithAlpaca = true;
    expect(changedSettings(current, after)).toEqual([
      { path: 'mqtt.broker', from: 'broker.lan', to: '192.168.1.10' },
      { path: 'alerts.sendMode', from: 'any', to: 'whileConnected' },
    ]);
  });

  it('lists settings an older firmware lacked as kept, a whole section as one path', () => {
    const file = copy();
    delete file.wind;
    delete ((file.mqtt as Settings).publish as Settings).diagnostics;
    expect(missingPaths(current, file)).toEqual(['mqtt.publish.diagnostics', 'wind']);
  });

  it('lists settings this firmware does not know as ignored', () => {
    const file = { ...copy(), skySensors: { second: true } };
    expect(previewDetails(current, file, preview(copy()), facts).ignored).toEqual(['skySensors']);
  });

  it('warns about another WiFi network, a new local name and the device password', () => {
    const after = copy();
    (after.wifi as Settings).ssid = 'Elsewhere';
    (after.wifi as Settings).hostname = 'roof';
    after.auth = { enabled: true, username: 'admin', password: '********' };
    expect(restoreWarnings(current, preview(after), facts)).toEqual([
      { code: 'wifi-network', ssid: 'Elsewhere' },
      { code: 'device-password' },
      { code: 'address', hostname: 'roof' },
    ]);
  });

  it('warns about the imaging app only when the restore restarts the device', () => {
    const connected = { ...facts, imagingAppConnected: true };
    expect(restoreWarnings(current, preview(copy()), connected)).toEqual([]);
    expect(restoreWarnings(current, preview(copy(), { restart: ['wifi'] }), connected)).toEqual([{ code: 'imaging-app' }]);
  });

  it('warns when the backup comes from newer firmware', () => {
    const newer = preview(copy(), { backup: { format: 1, firmware: '0.9.0-beta.1' } });
    expect(restoreWarnings(current, newer, facts)).toEqual([{ code: 'newer-firmware', firmware: '0.9.0-beta.1' }]);
    expect(restoreWarnings(current, preview(copy(), { backup: { format: 1, firmware: '0.2.0' } }), facts)).toEqual([]);
  });

  it('counts a secret that changes as a change', () => {
    const p = preview(copy(), { secrets: { state: 'restored', changed: ['wifi.password'], unchanged: [] } });
    expect(changesNothing(previewDetails(current, copy(), p, facts), p)).toBe(false);
  });

  it('groups top-level settings under general', () => {
    expect(sectionOf('deviceName')).toBe('general');
    expect(sectionOf('mqtt.publish.sky')).toBe('mqtt');
  });
});
