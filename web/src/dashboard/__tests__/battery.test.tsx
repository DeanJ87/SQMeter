import { describe, expect, it } from 'vitest';
import { render, screen } from '@testing-library/preact';
import { generateSensorData, mockConfig, mockStatus } from '../../mocks/data';
import { mockBatteryAbsent, mockBatteryPresent } from '../../mocks/battery';
import type { BatteryReading, SensorData, SystemStatus } from '../../types';
import type { EffectiveReport } from '../../lib/settingsDeps';
import { dashboardCards, type CardContext } from '../../components/dashboard/cards';
import { glanceItems, type GlanceInput } from '../glance';

// The battery on the dashboard (spec 032 FR-014, contracts/ui.md, mockups b1-b8 and the glance row).

const sensorsWith = (battery: Partial<BatteryReading> | BatteryReading): SensorData => ({
  ...generateSensorData(),
  battery: { ...mockBatteryPresent(), ...battery },
});

const ctx = (sensors: SensorData): CardContext => ({
  sensors,
  status: structuredClone(mockStatus),
  config: structuredClone(mockConfig),
  quiet: false,
  connected: true,
  sqmHistory: [],
});

const show = (battery: Partial<BatteryReading> | BatteryReading) => {
  const entry = dashboardCards(ctx(sensorsWith(battery))).find((item) => item.id === 'battery');
  if (entry) render(<>{entry.node}</>);
  return entry;
};

const tile = (label: string) => screen.getByText(label).closest('.metric-tile')?.querySelector('.metric-value')?.textContent;

describe('Battery card (spec 032)', () => {
  // inventory: battery-card
  it('shows a discharging pack: percent, state, voltage and time left', () => {
    show({});
    expect(screen.getByText('Battery')).toBeInTheDocument();
    expect(screen.getByText('82')).toBeInTheDocument();
    expect(screen.getByText('Discharging')).toBeInTheDocument();
    expect(tile('Voltage')).toBe('3.91');
    expect(tile('Time left')).toBe('13h 17m');
    expect(screen.queryByText('Power saving')).not.toBeInTheDocument();
  });

  it('has no card without a gauge', () => {
    expect(show(mockBatteryAbsent)).toBeUndefined();
    const sensors = generateSensorData();
    delete sensors.battery; // older firmware
    expect(dashboardCards(ctx(sensors)).some((item) => item.id === 'battery')).toBe(false);
  });

  it('charging: time to full, "--" while unknown, "> 99 h" at the cap', () => {
    show({ state: 'charging', timeToEmptyMinutes: null, timeToFullMinutes: 185 });
    expect(screen.getByText('Charging')).toBeInTheDocument();
    expect(tile('Time to full')).toBe('3h 5m');
    expect(screen.queryByText('Time left')).not.toBeInTheDocument();
    document.body.innerHTML = '';
    show({ state: 'charging', timeToFullMinutes: null });
    expect(tile('Time to full')).toBe('--');
    document.body.innerHTML = '';
    show({ state: 'discharging', timeToEmptyMinutes: 5940 });
    expect(tile('Time left')).toBe('> 99 h');
  });

  it('charged: voltage only', () => {
    show({ state: 'charged', percent: 100, timeToEmptyMinutes: null });
    expect(screen.getByText('Charged')).toBeInTheDocument();
    expect(screen.getByText('100')).toBeInTheDocument();
    expect(screen.queryByText('Time left')).not.toBeInTheDocument();
    expect(screen.queryByText('Time to full')).not.toBeInTheDocument();
  });

  it('a charging fault says why behind "?"', () => {
    show({ state: 'fault', fault: 'temperature' });
    expect(screen.getByText('Charging fault')).toBeInTheDocument();
    expect(screen.getByRole('tooltip')).toHaveTextContent('Too cold or too hot to charge');
  });

  it('no battery on USB: no percent', () => {
    show({ state: 'no-battery', percent: null, timeToEmptyMinutes: null });
    expect(screen.getByText('No battery')).toBeInTheDocument();
    expect(screen.queryByText('82')).not.toBeInTheDocument();
  });

  it('estimating: no percent yet, times "--"', () => {
    show({ estimating: true, percent: null });
    expect(screen.getByText('Estimating')).toBeInTheDocument();
    expect(tile('Time left')).toBe('--');
  });

  it('power saving adds a second pill', () => {
    show({ powerSaving: true });
    expect(screen.getByText('Power saving')).toBeInTheDocument();
    expect(screen.getByText('Discharging')).toBeInTheDocument();
  });

  it('a gauge that stopped answering keeps its card, with the charger state', () => {
    show({ present: true, status: 'missing', state: 'no-battery', usbPower: true } as BatteryReading);
    expect(screen.getByText('Battery')).toBeInTheDocument();
    expect(screen.getByText('Not responding')).toBeInTheDocument();
    expect(screen.getByText('No battery')).toBeInTheDocument();
    expect(screen.queryByText('Voltage')).not.toBeInTheDocument();
  });
});

const glance = (battery: Partial<BatteryReading> | BatteryReading, status: Partial<SystemStatus> = {}) => {
  const input: GlanceInput = {
    sensors: { ...sensorsWith(battery), timeValid: true, dataStale: false },
    status: { ...structuredClone(mockStatus), ...status },
    config: structuredClone(mockConfig),
    effective: { facts: {} as EffectiveReport['facts'], settings: [] },
    connected: true,
    quiet: false,
    schedule: { armed: true, mode: 'any', reason: 'none', since: null, sinceAgeMs: null },
  };
  return glanceItems(input).find((item) => item.id === 'battery');
};

describe('Battery glance tile (spec 032)', () => {
  // inventory: battery-glance
  it('none while the pack is fine, or without a gauge', () => {
    expect(glance({})).toBeUndefined();
    expect(glance(mockBatteryAbsent)).toBeUndefined();
  });

  it('warns below the low level', () => {
    expect(glance({ percent: 18.4 })).toMatchObject({ severity: 'note', label: 'Battery', state: '18%' });
  });

  it('is bad below the critical level or on a charging fault', () => {
    expect(glance({ percent: 8 })).toMatchObject({ severity: 'problem', state: '8%' });
    expect(glance({ state: 'fault', fault: 'temperature' })).toMatchObject({ severity: 'problem', state: 'Charging fault' });
  });

  it('is bad while the device is about to sleep', () => {
    const battery = { ...mockBatteryPresent(), percent: 4, shutdownInSeconds: 20 };
    expect(glance({ percent: 4 }, { battery })).toMatchObject({ severity: 'problem', state: 'Going to sleep' });
  });

  it('never fires on estimating or stale data', () => {
    expect(glance({ percent: null, estimating: true })).toBeUndefined();
    expect(glance({ present: true, status: 'stale' } as BatteryReading)).toBeUndefined();
  });
});
