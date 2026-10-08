// @vitest-environment node
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import Ajv from 'ajv';
import { demoDevice } from '../device';
import { statusDocument } from '../handlers';

// Every document the demo answers must match the device's contract
// (specs/016-demo-device-emulation/contracts/schemas, generated from a real
// SQMeter). A field the device doesn't send, or a missing one, fails here.

const files = import.meta.glob('../../../../specs/016-demo-device-emulation/contracts/schemas/*.schema.json', {
  eager: true,
  import: 'default',
});
const ajv = new Ajv({ allErrors: true, strict: false });
const schemas: Record<string, object> = Object.fromEntries(
  Object.entries(files).map(([path, schema]) => [path.split('/').pop()!.replace('.schema.json', ''), schema as object]),
);

const check = (schemaName: string, document: unknown) => {
  const validate = ajv.compile(schemas[schemaName]);
  const ok = validate(document);
  expect(ok, `${schemaName}: ${ajv.errorsText(validate.errors, { separator: '\n' })}`).toBe(true);
};

const alpaca = (path: string) => JSON.parse(demoDevice.alpaca('GET', path, []).body);

describe('demo documents match the device contract', () => {
  beforeAll(async () => {
    await demoDevice.start();
    await new Promise((resolve) => setTimeout(resolve, 2100)); // a couple of ticks
  });
  afterAll(() => demoDevice.stop());

  it('readings (/api/sensors, /ws/sensors)', () => check('readings', JSON.parse(demoDevice.readings())));
  it('status (/api/status, /ws/status)', () => check('status', statusDocument()));
  it('safety', () => check('safety', JSON.parse(demoDevice.safety())));
  it('settings in effect (specs/020-settings-dependencies)', () => check('settings-effective', JSON.parse(demoDevice.effective())));
  it('config', () => check('config', JSON.parse(demoDevice.getConfig())));
  it('recent alerts', () => check('alerts-recent', JSON.parse(demoDevice.recentAlerts())));
  it('safety history', () => check('safety-history', JSON.parse(demoDevice.safetyHistory())));
  it('Alpaca management and device state', () => {
    check('alpaca-description', alpaca('/management/v1/description'));
    check('alpaca-configured-devices', alpaca('/management/v1/configureddevices'));
    check('alpaca-devicestate', alpaca('/api/v1/safetymonitor/0/devicestate'));
    check('alpaca-devicestate', alpaca('/api/v1/observingconditions/0/devicestate'));
  });

  it('a field the device does not send fails the check', () => {
    const readings = JSON.parse(demoDevice.readings());
    readings.sky.sqmValue = readings.sky.sqm; // e.g. a rename that drifted
    const validate = ajv.compile(schemas.readings);
    expect(validate(readings)).toBe(false);
  });
});
