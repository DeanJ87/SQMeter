import { describe, it, expect } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import SafetyCard, { describeHistoryEntry } from '../SafetyCard';

const safety = { safe: false, rawSafe: false, alpacaEnabled: true, reasonFlags: 48, reasons: ['Cloud 62% >= 35%'], secondsUntilSafe: 0, evaluatedAgeMs: 0, changedAgeMs: 1000 };

describe('Safety history', () => {
  it('describes restarts, holds, reasons and alerts', () => {
    expect(describeHistoryEntry({ kind: 'boot', boot: 1, uptime: 0, resetReason: 3 })).toBe('Device restarted (settings, update or restart)');
    expect(describeHistoryEntry({ kind: 'change', boot: 1, uptime: 1, safe: false, held: true, reasonFlags: 0 })).toBe('Unsafe - waiting out the safe delay');
    expect(describeHistoryEntry({ kind: 'change', boot: 1, uptime: 1, safe: false, reasonFlags: 0x30 })).toBe('Unsafe: cloud, SQM');
    expect(describeHistoryEntry({ kind: 'alert', boot: 1, uptime: 1, safe: true })).toBe('Alert sent: safe');
  });

  it('loads the history from the device on demand', async () => {
    render(<SafetyCard safety={safety as any} />);
    fireEvent.click(screen.getByRole('button', { name: 'History' }));
    expect(await screen.findByText('Unsafe: cloud, SQM')).toBeInTheDocument();
    expect(screen.getByText('Device restarted (settings, update or restart)')).toBeInTheDocument();
    expect(screen.getByText('1s after start')).toBeInTheDocument();
  });
});
