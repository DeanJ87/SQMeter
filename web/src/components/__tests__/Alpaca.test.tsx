import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/preact';
import Alpaca from '../Alpaca';
import { describeClient } from '../../lib/alpacaClients';

describe('Alpaca', () => {
  it('lists configured devices with their setup URLs and live device state', async () => {
    render(<Alpaca />);

    expect(await screen.findByText('SQMeter SafetyMonitor')).toBeInTheDocument();
    expect(screen.getByText('SQMeter ObservingConditions')).toBeInTheDocument();
    expect(
      screen.getByText(`${window.location.origin}/setup/v1/observingconditions/0/setup`)
    ).toBeInTheDocument();
    expect(await screen.findByText('IsSafe')).toBeInTheDocument();
    expect(await screen.findByText('CloudCover')).toBeInTheDocument();
  });
});

describe('Alpaca imaging app state (specs/021)', () => {
  it('shows whether an imaging app is checking each device', async () => {
    render(<Alpaca />);
    expect(await screen.findByText('Connected, last checked 2 s ago')).toBeInTheDocument();
    expect(screen.getByText('Waiting for an imaging app')).toBeInTheDocument();
  });

  it('describes every state', () => {
    const base = { connected: false, watching: false, silent: false, lastCheckedAgeMs: null, clientId: null };
    expect(describeClient({ ...base, connected: true, watching: true, lastCheckedAgeMs: 3000 }).text).toBe('Connected, last checked 3 s ago');
    expect(describeClient({ ...base, connected: true, watching: true, silent: true, lastCheckedAgeMs: 180000 })).toEqual({
      text: 'Gone quiet, last checked 3 min ago',
      tone: 'warn',
    });
    expect(describeClient({ ...base, watching: true, lastCheckedAgeMs: 1000 }).text).toBe('Checked without connecting, last checked 1 s ago');
    expect(describeClient({ ...base, lastCheckedAgeMs: 7200000 }).text).toBe('Disconnected, last checked 2 h 0 min ago');
    expect(describeClient(base).text).toBe('Waiting for an imaging app');
  });
});
