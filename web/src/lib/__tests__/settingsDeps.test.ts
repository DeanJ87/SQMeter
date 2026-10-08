import { describe, expect, it } from 'vitest';
import type { Config } from '../../types';
import { REASONS, evaluate, effectiveEntries, newlyInactive, viewOf, type DepFacts } from '../settingsDeps';
import catalogueJson from '../../../../lib/SettingsDeps/catalogue.json';
import casesJson from '../../../../test/fixtures/settings-deps/cases.json';
import defaultConfigJson from '../../../../test/fixtures/settings-deps/default-config.json';

// The device's rules (lib/SettingsDeps) and this mirror must give the same
// answers: both run test/fixtures/settings-deps/cases.json
// (specs/020-settings-dependencies). Catalogue IDs covered: D-01 D-02 D-03
// D-04 D-05 D-06 D-07 D-08 D-09 D-10 D-11 D-12 D-13 D-14 D-15 D-16 D-17 D-18
// D-19 D-23 D-24 D-25 D-26 D-28 D-29 D-30 D-31 D-32 D-35 D-36.

type Json = Record<string, unknown>;
const merge = (base: Json, overlay: Json): Json => {
  const out: Json = { ...base };
  for (const [key, value] of Object.entries(overlay)) {
    out[key] = value && typeof value === 'object' && !Array.isArray(value) && typeof base[key] === 'object' ? merge(base[key] as Json, value as Json) : value;
  }
  return out;
};

const defaults = defaultConfigJson as unknown as Config;
const fixtures = casesJson as unknown as {
  baseFacts: DepFacts;
  cases: { name: string; config: Json; facts?: Partial<DepFacts>; expect: Record<string, Json | null> }[];
};
const catalogue = catalogueJson as unknown as {
  reasons: Record<string, { text: string; fix: string }>;
  entries: { id: string; kind: string; settings: string[] }[];
};

describe('settings dependencies: same answers as the device', () => {
  it.each(fixtures.cases.map((c) => [c.name, c] as const))('%s', (_name, c) => {
    const config = merge(defaults as unknown as Json, c.config) as unknown as Config;
    const view = viewOf(evaluate(config, { ...fixtures.baseFacts, ...c.facts }));
    for (const [setting, expected] of Object.entries(c.expect)) {
      const entry = view.get(setting);
      if (expected === null) {
        expect(entry.state, setting).toBe('unknown'); // not reported
        continue;
      }
      expect({ setting, state: entry.state, id: entry.id }).toEqual({ setting, state: expected.state, id: expected.id });
      if (entry.state === 'inactive') {
        expect(entry.reason, setting).toBe(expected.reason);
        if (expected.text) expect(entry.text, setting).toBe(expected.text);
        if (expected.fix) expect(entry.fix, setting).toBe(expected.fix);
      } else {
        expect(expected.reason, setting).toBeUndefined();
      }
      expect(entry.unmet ?? '', setting).toBe(expected.unmet ?? '');
      expect(entry.neutral ?? false, setting).toBe(expected.neutral ?? false);
    }
  });

  it('uses the catalogue reasons and reports every catalogued setting', () => {
    expect(REASONS).toEqual(catalogue.reasons);
    const catalogued = new Set(catalogue.entries.filter((e) => e.kind === 'setting').flatMap((e) => e.settings));
    const reported = new Set(evaluate(defaults, fixtures.baseFacts).map((e) => e.setting));
    expect([...reported].sort()).toEqual([...catalogued].sort());
  });
});

describe('a fresh device (SC-005)', () => {
  it('shows no inactive warnings with the shipped defaults - only neutral notes', () => {
    // A standard build that has just joined WiFi: no Bluetooth, GPS off, no location yet.
    const standard: DepFacts = { ...fixtures.baseFacts, bluetoothBuild: false, bluetoothRunning: false, pairedPhones: 0, gpsRunning: false, gpsFix: false };
    const warnings = evaluate(defaults, standard).filter((e) => e.state === 'inactive' && !e.neutral && e.reason !== 'alerts-off');
    // "Alerts are off" is shown once, by the Send alerts switch, not as a warning per row.
    expect(warnings.map((e) => `${e.setting}: ${e.text}`)).toEqual([]);
  });
});

describe('before the device has reported', () => {
  it('evaluates settings-only links and leaves runtime links unknown (D-01, D-03)', () => {
    const config = merge(defaults as unknown as Json, { alerts: { enabled: true, mqtt: { enabled: true }, pushover: { enabled: true } } }) as unknown as Config;
    const view = viewOf(evaluate(config, null));
    expect(view.get('alerts.mqtt.enabled')).toMatchObject({ state: 'inactive', reason: 'mqtt-off' });
    expect(view.get('alerts.pushover.enabled').state).toBe('unknown');
    expect(view.reason('alerts.pushover.enabled')).toBeNull();
  });
});

describe('previewing unsaved changes (FR-006)', () => {
  const on = merge(defaults as unknown as Json, {
    alerts: { enabled: true, mqtt: { enabled: true } },
    mqtt: { enabled: true, broker: '192.168.1.10', topic: 'sqmeter' },
  }) as unknown as Config;
  const report = { facts: fixtures.baseFacts, settings: evaluate(on, fixtures.baseFacts) };

  it("shows the device's report for settings that are on while the form is clean", () => {
    const fake = [{ id: 'D-02', setting: 'alerts.mqtt.enabled', state: 'inactive' as const, reason: 'mqtt-disconnected' }];
    const view = viewOf(effectiveEntries(on, { ...report, settings: fake }, false));
    expect(view.get('alerts.mqtt.enabled')).toBe(fake[0]);
    expect(view.get('alerts.pushover.enabled').state).toBe('off'); // local
  });

  it('says what would block a setting that is off (D-16)', () => {
    const entry = viewOf(evaluate(defaults, fixtures.baseFacts)).get('alpaca.windSpeedUnsafeEnabled');
    expect(entry.state).toBe('off');
    expect(entry.blockedBy).toMatchObject({ id: 'D-16', reason: 'wind-off', text: 'Anemometer is off' });
  });

  it('lists what saving would make inactive (D-01)', () => {
    const draft = merge(on as unknown as Json, { mqtt: { enabled: false } }) as unknown as Config;
    const preview = newlyInactive(report.settings, effectiveEntries(draft, report, true));
    expect(preview.map((e) => e.setting)).toContain('alerts.mqtt.enabled');
    expect(preview.every((e) => !e.neutral)).toBe(true); // publish groups going quiet aren't news
  });
});
