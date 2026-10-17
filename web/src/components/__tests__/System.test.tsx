import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/preact';
import System from '../System';
import type { SystemStatus } from '../../types';
import { mockStatus } from '../../mocks/data';

vi.mock('../../hooks/useWebSocket', () => ({
  useWebSocket: vi.fn(),
}));

import { useWebSocket } from '../../hooks/useWebSocket';

describe('System', () => {
  it('shows a loading state while disconnected', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: null, connected: false, lastMessageAt: null });

    render(<System />);

    expect(screen.getByText('Loading...')).toBeInTheDocument();
  });

  it('renders firmware version once connected', () => {
    vi.mocked(useWebSocket<SystemStatus>).mockReturnValue({
      data: mockStatus,
      connected: true,
      lastMessageAt: Date.now(),
    });

    render(<System />);

    expect(screen.getByText(`v${mockStatus.firmware!.version}`)).toBeInTheDocument();
  });

  it('lists the IPv6 addresses with their scope (spec 015)', () => {
    vi.mocked(useWebSocket<SystemStatus>).mockReturnValue({
      data: {
        ...mockStatus,
        wifi: {
          ...mockStatus.wifi,
          ipv6: {
            enabled: true,
            addresses: [
              { address: 'fe80::a00:27ff:fe4e:66a1', scope: 'link-local' },
              { address: '2a02:8010:abcd:1::9', scope: 'global' },
            ],
          },
        },
      },
      connected: true,
      lastMessageAt: Date.now(),
    });

    render(<System />);

    expect(screen.getByText('IPv6 (link-local)')).toBeInTheDocument();
    expect(screen.getByText('fe80::a00:27ff:fe4e:66a1')).toBeInTheDocument();
    expect(screen.getByText('IPv6 (global)')).toBeInTheDocument();
    expect(screen.getByText('2a02:8010:abcd:1::9')).toBeInTheDocument();
  });
});

describe('System - display row (spec 031)', () => {
  const show = (display?: SystemStatus['display']) => {
    vi.mocked(useWebSocket<SystemStatus>).mockReturnValue({ data: { ...mockStatus, display }, connected: true, lastMessageAt: Date.now() });
    render(<System />);
  };

  it.each([
    [{ enabled: true, detected: true, on: true, page: 'sky' as const, nightMode: false }, 'Detected, on'],
    [{ enabled: true, detected: true, on: false, page: null, nightMode: false }, 'Detected, off'],
    [{ enabled: true, detected: false, on: false, page: null, nightMode: false }, 'Not detected'],
    [{ enabled: false, detected: false, on: false, page: null, nightMode: false }, 'Off'],
  ])('shows the display state', (display, text) => {
    show(display);
    const row = screen.getByText('Display').closest('.reading-row') as HTMLElement;
    expect(row.textContent).toContain(text);
  });

  it('has no row on firmware without a display', () => {
    show(undefined);
    expect(screen.queryByText('Display')).not.toBeInTheDocument();
  });
});
