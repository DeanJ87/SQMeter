import { describe, it, expect, beforeEach } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen } from '@testing-library/preact';
import AlertsBell from '../AlertsBell';
import { mockRecentAlerts } from '../../mocks/data';
import { server } from '../../test/mswServer';

describe('AlertsBell', () => {
  beforeEach(() => {
    try {
      localStorage.clear();
    } catch {
      // ignore
    }
  });

  it('is hidden while alerts are off', async () => {
    server.use(http.get('/api/alerts/recent', () => HttpResponse.json({ enabled: false, alerts: mockRecentAlerts })));
    const { container } = render(<AlertsBell />);
    await new Promise((resolve) => setTimeout(resolve, 50));
    expect(container.querySelector('.alerts-bell')).toBeNull();
  });

  it('shows unread alerts and clears the count when opened', async () => {
    render(<AlertsBell />);
    const button = await screen.findByRole('button', { name: /Alerts, 2 new/ });
    fireEvent.click(button);
    expect(screen.getByText('Observatory UNSAFE')).toBeInTheDocument();
    expect(screen.getAllByText(/pushover: sent/).length).toBe(2);
    expect(screen.getByRole('button', { name: 'Alerts' })).toBeInTheDocument();
  });
});
