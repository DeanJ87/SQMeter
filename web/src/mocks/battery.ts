import type { BatteryDiagnostics, BatteryReading } from '../types';

// The battery (spec 032): a travel board on its pack, and a board without a gauge.
export const mockBatteryPresent = (): BatteryReading => ({
  present: true,
  status: 'ok',
  ageMs: 1200,
  estimating: false,
  percent: 82.4,
  voltage: 3.912,
  ratePercentPerHour: -6.2,
  state: 'discharging',
  usbPower: false,
  fault: null,
  timeToEmptyMinutes: 797,
  timeToFullMinutes: null,
  powerSaving: false,
});
export const mockBatteryAbsent: BatteryReading = { present: false };

export const mockBatteryDiagnostics: BatteryDiagnostics = {
  version: '0x0012',
  vcellRaw: 50074,
  socRaw: 21094,
  crateRaw: -30,
  statusRaw: 0,
  resetSeen: false,
  estimating: false,
  charger: { stat1: true, stat2: true, pg: true },
  gaugeAlert: false,
  policy: 'normal',
  currentMa: null,
};
