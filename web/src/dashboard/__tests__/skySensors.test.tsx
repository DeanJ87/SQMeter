import { describe, expect, it } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import { generateSensorData, mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';
import type { Config, SensorData, SkySensorReading, SystemStatus } from '../../types';
import { dashboardCards, type CardContext } from '../../components/dashboard/cards';
import { authoritativeMissing, nextSqmHistory, otherReading, otherSkyFault, skyChoice, type SqmHistory } from '../skySensors';

// Two sky sensors on the dashboard (spec 030 FR-010, FR-012, contracts/ui.md).

const reading = (sqm: number): SkySensorReading => ({ status: 'ok', ageMs: 400, sqm, nelm: 6.1, bortle: 2 });

const sensorsWith = (sqm: number, sqml: number, change: (s: SensorData) => void = () => undefined): SensorData => {
  const s = generateSensorData();
  s.sky = { ...s.sky, status: 'ok', source: 'sqm', sqm, bortle: 4 };
  s.skySensors = { sqm: reading(sqm), sqml: reading(sqml), deltaSqm: Math.round((sqml - sqm) * 100) / 100 };
  change(s);
  return s;
};

const configWith = (sky: Partial<NonNullable<Config['sky']>>): Config => ({
  ...structuredClone(mockConfig),
  sky: { ...mockConfig.sky!, ...sky },
});

const one = configWith({ sqmlEnabled: false });

const ctx = (sensors: SensorData, config: Config, status: SystemStatus | null = structuredClone(mockStatus)): CardContext => ({
  sensors,
  status,
  config,
  quiet: false,
  connected: true,
  sqmHistory: [],
});

const card = (c: CardContext, id: string) => {
  const entry = dashboardCards(c).find((item) => item.id === id);
  if (!entry) return null;
  render(<>{entry.node}</>);
  return entry;
};

describe('which sky sensor counts', () => {
  it('defaults to the SQM alone for firmware and configs without spec 030', () => {
    const { sky: _sky, ...older } = mockConfig;
    expect(skyChoice(older as Config)).toMatchObject({ authoritative: 'sqm', both: false, named: false, showOther: true });
    expect(skyChoice(null).enabled).toEqual({ sqm: true, sqml: false });
  });

  it('names the sensors once the SQM-L is on or still chosen', () => {
    expect(skyChoice(configWith({})).named).toBe(true);
    expect(skyChoice(configWith({ sqmlEnabled: false, authoritative: 'sqml' })).named).toBe(true);
    expect(skyChoice(one).named).toBe(false);
  });
});

describe('the other reading and the difference (FR-006, FR-012)', () => {
  it('shows the other reading and SQM-L - SQM when both report', () => {
    expect(otherReading(sensorsWith(21.32, 21.56), configWith({ authoritative: 'sqml' }))).toEqual({
      role: 'sqm',
      sqm: 21.32,
      difference: 0.24,
      warn: false,
    });
  });

  it('works out the difference when the device sends none', () => {
    const s = sensorsWith(21.0, 21.5, (x) => delete x.skySensors!.deltaSqm);
    expect(otherReading(s, configWith({}))?.difference).toBeCloseTo(0.5);
  });

  it('flags a difference above 1.5 mag/arcsec²', () => {
    expect(otherReading(sensorsWith(19.8, 21.6), configWith({}))?.warn).toBe(true);
    expect(otherReading(sensorsWith(21.6, 20.0), configWith({}))?.warn).toBe(true);
  });

  it('hides it with "Show other reading" off, with one sensor, or when the other sensor is not ok', () => {
    expect(otherReading(sensorsWith(21, 21.2), configWith({ showOther: false }))).toBeNull();
    expect(otherReading(sensorsWith(21, 21.2), one)).toBeNull();
    const failed = sensorsWith(21, 21.2, (s) => (s.skySensors!.sqml = { status: 'error' }));
    expect(otherReading(failed, configWith({}))).toBeNull();
  });
});

describe('a missing authoritative sensor (FR-010)', () => {
  const noSky = (change: (s: SensorData) => void) =>
    sensorsWith(21, 21.2, (s) => {
      s.light = { status: 'error' };
      s.sky = { status: 'error', source: 'sqml' };
      change(s);
    });

  it('names the sensor and offers the other one when it reports', () => {
    const s = noSky((x) => (x.skySensors!.sqml = { status: 'missing' }));
    expect(authoritativeMissing(s, configWith({ authoritative: 'sqml' }))).toEqual({ role: 'sqml', health: 'missing', switchTo: 'sqm' });
  });

  it('says a switched-off authoritative sensor is off', () => {
    const s = noSky((x) => delete x.skySensors!.sqml);
    expect(authoritativeMissing(s, configWith({ authoritative: 'sqml', sqmlEnabled: false }))).toEqual({
      role: 'sqml',
      health: 'off',
      switchTo: 'sqm',
    });
  });

  it('offers no switch when the other sensor is not reporting', () => {
    const s = noSky((x) => {
      x.skySensors!.sqml = { status: 'error' };
      x.skySensors!.sqm = { status: 'stale' };
    });
    expect(authoritativeMissing(s, configWith({ authoritative: 'sqml' }))?.switchTo).toBeNull();
  });

  it('leaves a one-sensor device as before', () => {
    expect(
      authoritativeMissing(
        noSky(() => undefined),
        one,
      ),
    ).toBeNull();
  });
});

describe('the other sensor once it has failed (spec 025 D6)', () => {
  it('keeps a fault card after an error or a stale reading', () => {
    const status = structuredClone(mockStatus);
    status.skySensors!.sqml = { status: 'error', detected: false };
    expect(otherSkyFault(sensorsWith(21, 21.2), status, configWith({}))).toEqual({ role: 'sqml', health: 'error' });
  });

  it('has none for a sensor that never answered since boot, or a disabled one', () => {
    const status = structuredClone(mockStatus);
    status.skySensors!.sqml = { status: 'missing', detected: false };
    expect(otherSkyFault(sensorsWith(21, 21.2), status, configWith({}))).toBeNull();
    status.skySensors!.sqml = { status: 'error', detected: false };
    expect(otherSkyFault(sensorsWith(21, 21.2), status, one)).toBeNull();
  });
});

describe('the trend under the big value', () => {
  it('restarts when the authoritative sensor changes', () => {
    let history: SqmHistory = { source: 'sqm', values: [] };
    history = nextSqmHistory(history, 'sqm', 21.1);
    history = nextSqmHistory(history, 'sqm', 21.2);
    expect(history.values).toEqual([21.1, 21.2]);
    const switched = nextSqmHistory(history, 'sqml', 21.5);
    expect(switched).toEqual({ source: 'sqml', values: [21.5] });
  });

  it('keeps the last 24 readings', () => {
    let history: SqmHistory = { source: 'sqm', values: [] };
    for (let i = 0; i < 30; i++) history = nextSqmHistory(history, 'sqm', 20 + i / 100);
    expect(history.values).toHaveLength(24);
  });
});

describe('the Sky Quality card', () => {
  it('is unchanged with one sensor: no name pill, no extra tiles', () => {
    // inventory: sky-source  inventory: sky-sensors
    card(ctx(sensorsWith(21.32, 21.56), one), 'sky');
    expect(screen.queryByText('SQM')).toBeNull();
    expect(screen.queryByText('Difference')).toBeNull();
    expect(document.querySelectorAll('.metric-tile')).toHaveLength(3);
  });

  it('carries the name pill and the other reading and difference with both on (m1)', () => {
    const s = sensorsWith(21.32, 21.56, (x) => (x.sky = { ...x.sky, source: 'sqml', sqm: 21.56 }));
    card(ctx(s, configWith({ authoritative: 'sqml' })), 'sky');
    expect(within(document.querySelector('.hero-pills') as HTMLElement).getByText('SQM-L')).toBeInTheDocument();
    const tiles = [...document.querySelectorAll('.metric-tile')].map((tile) => tile.textContent);
    expect(tiles).toContain('SQM21.32mag / arcsec²');
    expect(tiles).toContain('Difference+0.24mag / arcsec²');
  });

  it('shows the name pill only with "Show other reading" off (m2)', () => {
    card(ctx(sensorsWith(21.32, 21.56), configWith({ showOther: false })), 'sky');
    expect(within(document.querySelector('.hero-pills') as HTMLElement).getByText('SQM')).toBeInTheDocument();
    expect(screen.queryByText('Difference')).toBeNull();
  });

  it('colours a difference above 1.5 amber (m3)', () => {
    card(ctx(sensorsWith(19.8, 21.6), configWith({})), 'sky');
    const difference = screen.getByText('Difference').closest('.metric-tile') as HTMLElement;
    expect(difference.querySelector('.metric-value')?.className).toContain('tone-amber');
  });

  it('names a missing authoritative sensor and switches to the other on request (m4, FR-010)', async () => {
    // inventory: sky-authoritative-missing
    let body: unknown = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        body = await request.json();
        return HttpResponse.json({ success: true });
      }),
    );
    const s = sensorsWith(21.32, 21.56, (x) => {
      x.light = { status: 'missing' };
      x.sky = { status: 'missing', source: 'sqml' };
      x.skySensors!.sqml = { status: 'missing' };
    });
    card(ctx(s, configWith({ authoritative: 'sqml' })), 'sky');
    expect(screen.getByText('Not responding')).toBeInTheDocument();
    expect(screen.getByText('SQM-L not responding.')).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Use SQM' }));
    await waitFor(() => expect(body).toEqual({ sky: { authoritative: 'sqm' } }));
  });

  it('keeps the plain unavailable card on a one-sensor device', () => {
    const s = sensorsWith(21, 21, (x) => {
      x.light = { status: 'missing' };
      x.sky = { status: 'missing' };
    });
    const status = structuredClone(mockStatus);
    status.sensors.light = { status: 'missing' };
    card(ctx(s, one, status), 'sky');
    expect(screen.getByText('Not responding')).toBeInTheDocument();
    expect(screen.queryByRole('button')).toBeNull();
  });

  it('gives the failed other sensor its own fault card, titled with its name (m5)', () => {
    // inventory: sky-other-fault
    const status = structuredClone(mockStatus);
    status.skySensors!.sqml = { status: 'error', detected: false };
    const entry = card(
      ctx(
        sensorsWith(21, 21.2, (x) => (x.skySensors!.sqml = { status: 'error' })),
        configWith({}),
        status,
      ),
      'sky-other',
    );
    expect(entry?.title).toBe('SQM-L');
    expect(screen.getByRole('heading', { name: 'SQM-L' })).toBeInTheDocument();
    expect(screen.getByText('Error')).toBeInTheDocument();
    status.skySensors!.sqml = { status: 'missing', detected: false };
    expect(dashboardCards(ctx(sensorsWith(21, 21.2), configWith({}), status)).some((item) => item.id === 'sky-other')).toBe(false);
  });
});
