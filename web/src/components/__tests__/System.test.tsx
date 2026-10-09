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
