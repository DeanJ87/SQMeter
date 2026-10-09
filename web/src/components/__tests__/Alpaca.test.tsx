import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/preact';
import Alpaca from '../Alpaca';
import { describeClient } from '../../lib/alpacaClients';

describe('Alpaca', () => {
  it('lists configured devices with their setup URLs and live device state', async () => {
    render(<Alpaca />);

    expect(await screen.findByText('SQMeter SafetyMonitor')).toBeInTheDocument();
    expect(screen.getByText('SQMeter ObservingConditions')).toBeInTheDocument();
    expect(screen.getByText(`${window.location.origin}/setup/v1/observingconditions/0/setup`)).toBeInTheDocument();
    expect(await screen.findByText('IsSafe')).toBeInTheDocument();
    expect(await screen.findByText('CloudCover')).toBeInTheDocument();
  });
});

describe('Alpaca imaging app state (specs/021, 026 DS-21)', () => {
  it('shows whether an imaging app is checking each device: a pill and when it last checked', async () => {
    render(<Alpaca />);
    expect(await screen.findByText('Checked 2 s ago')).toBeInTheDocument();
    expect(screen.getByText('Connected')).toBeInTheDocument();
    expect(screen.getByText('Waiting')).toBeInTheDocument();
  });

  it('describes every state', () => {
    const base = { connected: false, watching: false, silent: false, lastCheckedAgeMs: null, clientId: null };
    expect(describeClient({ ...base, connected: true, watching: true, lastCheckedAgeMs: 3000 })).toEqual({
      state: 'Connected',
      checked: 'Checked 3 s ago',
      tone: 'ok',
    });
    expect(describeClient({ ...base, connected: true, watching: true, silent: true, lastCheckedAgeMs: 180000 })).toEqual({
      state: 'Gone quiet',
      checked: 'Checked 3 min ago',
      tone: 'warn',
    });
    expect(describeClient({ ...base, watching: true, lastCheckedAgeMs: 1000 }).state).toBe('Checking');
    expect(describeClient({ ...base, lastCheckedAgeMs: 7200000 })).toMatchObject({
      state: 'Disconnected',
      checked: 'Checked 2 h 0 min ago',
    });
    expect(describeClient(base)).toEqual({ state: 'Waiting', tone: 'muted' });
  });
});
