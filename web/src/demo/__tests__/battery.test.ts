// @vitest-environment node
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { advanceBattery, batteryPreset, DEFAULT_CONDITIONS } from '../conditions';
import { demoDevice } from '../device';

// The demo's travel-board battery (spec 032, research D13).

const HOUR = 3_600_000;

describe('simulated pack', () => {
  it('drains about 6 %/h on its own and fills about 12 %/h on USB', () => {
    expect(advanceBattery(DEFAULT_CONDITIONS, HOUR).battery.percent).toBeCloseTo(82 - 6.2, 5);
    const usb = { ...DEFAULT_CONDITIONS, battery: { ...DEFAULT_CONDITIONS.battery, usb: true } };
    expect(advanceBattery(usb, HOUR).battery.percent).toBeCloseTo(94, 5);
    expect(advanceBattery(usb, 10 * HOUR).battery.percent).toBe(100);
  });

  it('stops on a charger fault and never goes below empty', () => {
    const fault = { ...DEFAULT_CONDITIONS, battery: { percent: 40, usb: true, fault: true } };
    expect(advanceBattery(fault, HOUR)).toBe(fault);
    const low = { ...DEFAULT_CONDITIONS, battery: { percent: 1, usb: false, fault: false } };
    expect(advanceBattery(low, HOUR).battery.percent).toBe(0);
  });

  it('presets: battery low just below the low level, charge fault on USB', () => {
    expect(batteryPreset('batteryLow', DEFAULT_CONDITIONS, 25).battery).toEqual({ percent: 23, usb: false, fault: false });
    expect(batteryPreset('chargeFault', DEFAULT_CONDITIONS).battery).toMatchObject({ usb: true, fault: true });
  });
});

describe('emulated device battery', () => {
  const tick = () => new Promise((resolve) => setTimeout(resolve, 1100));
  const battery = () => JSON.parse(demoDevice.readings()).battery;

  beforeAll(async () => {
    await demoDevice.start();
    await tick();
  });
  afterAll(() => demoDevice.stop());

  it('reports the pack, discharging', () => {
    expect(battery()).toMatchObject({ present: true, status: 'ok', state: 'discharging', usbPower: false });
    expect(battery().voltage).toBeGreaterThan(3.3);
  });

  it('charges on USB, then a charger fault', async () => {
    demoDevice.setUsbPower(true);
    await tick();
    expect(battery()).toMatchObject({ state: 'charging', usbPower: true });
    demoDevice.setChargeFault(true);
    await tick();
    expect(battery()).toMatchObject({ state: 'fault', fault: 'temperature' });
    demoDevice.setUsbPower(false);
  });

  it('saves power below the critical level', async () => {
    demoDevice.setInput('battery.percent', 8);
    await tick();
    expect(battery()).toMatchObject({ state: 'discharging', powerSaving: true });
  });

  it('the battery rule turns the verdict unsafe below its limit', async () => {
    demoDevice.applyConfig(JSON.stringify({ alpaca: { batteryEnabled: true, batteryMinPercent: 10 } }));
    demoDevice.setInput('battery.percent', 8);
    await tick();
    expect(JSON.parse(demoDevice.safety()).reasons.join()).toMatch(/Battery [78]% < 10%/); // it drains as it ticks
  });
});
