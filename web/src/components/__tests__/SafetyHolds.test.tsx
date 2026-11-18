import { afterEach, describe, expect, it, vi } from 'vitest';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import SafetyCard, { describeHistoryEntry } from '../SafetyCard';
import type { SafetyStatus } from '../../types';

// Safety holds on the Safety Monitor card (specs/035-safety-holds FR-011).

const SMOKE = "Smoke: neighbour's fire, PM2.5 85 µg/m³";
const base: SafetyStatus = {
  safe: false,
  rawSafe: false,
  alpacaEnabled: true,
  reasonFlags: (1 << 17) | (1 << 4),
  reasons: ['Cloud 78% >= 40%', SMOKE],
  secondsUntilSafe: 0,
  evaluatedAgeMs: 0,
  changedAgeMs: 1000,
  holds: [{ id: 'smoke', reason: SMOKE, source: 'Home Assistant', sinceAgeMs: 60000 }],
};

describe('Safety holds', () => {
  afterEach(() => vi.restoreAllMocks());

  it('lists a hold once, after the other reasons, with who set it and Clear', () => {
    render(<SafetyCard safety={base} />);
    const items = screen.getAllByRole('listitem');
    expect(items[0]).toHaveTextContent('Cloud 78%');
    expect(items[1]).toHaveTextContent(SMOKE);
    expect(screen.getAllByText(SMOKE, { exact: false })).toHaveLength(1);
    expect(items[1]).toHaveTextContent(/Hold "smoke" · Home Assistant · since \d\d:\d\d/);
    expect(screen.getByRole('button', { name: 'Clear' })).toBeInTheDocument();
  });

  it('says when a hold lifts by itself', () => {
    const holds = [{ id: 'roof', reason: 'Roof not fully open', source: 'RoofRoof', sinceEpoch: 1800000000, expiresInMs: 3600000 }];
    render(<SafetyCard safety={{ ...base, reasons: ['Roof not fully open'], holds }} />);
    expect(screen.getByText(/Hold "roof" · RoofRoof · since \d\d:\d\d.* · lifts \d\d:\d\d/)).toBeInTheDocument();
  });

  it('clears that hold only, by id', async () => {
    const fetchMock = vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response('{"cleared":true}', { status: 200 }));
    render(<SafetyCard safety={base} />);
    fireEvent.click(screen.getByRole('button', { name: 'Clear' }));
    await waitFor(() => expect(fetchMock).toHaveBeenCalledWith('/api/safety/hold/smoke', { method: 'DELETE' }));
  });

  it('describes holds in the history', () => {
    expect(describeHistoryEntry({ kind: 'change', boot: 1, uptime: 1, safe: false, reasonFlags: 1 << 17, holds: ['Smoke'] })).toBe(
      'Unsafe: hold: Smoke',
    );
    expect(
      describeHistoryEntry({ kind: 'holdSet', boot: 1, uptime: 1, safe: false, reasonFlags: 0, holdId: 'roof', holds: ['Smoke', 'Roof'] }),
    ).toBe('Unsafe: hold: Smoke, hold: Roof');
    expect(describeHistoryEntry({ kind: 'holdCleared', boot: 1, uptime: 1, reasonFlags: 1 << 4, holdId: 'smoke', holds: ['Smoke'] })).toBe(
      'Hold cleared: Smoke (still unsafe: cloud)',
    );
    expect(describeHistoryEntry({ kind: 'holdCleared', boot: 1, uptime: 1, reasonFlags: 0, holdId: 'manual' })).toBe(
      'Hold cleared: manual override',
    );
  });
});
