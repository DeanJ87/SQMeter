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
});
