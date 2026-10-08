import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen } from '@testing-library/preact';
import Settings from '../Settings';
import { server } from '../../test/mswServer';

const recentAfterTest = (status: string, detail: string) => {
  let calls = 0;
  server.use(
    http.get('/api/alerts/recent', () => {
      calls += 1;
      const alerts =
        calls === 1
          ? []
          : [
              {
                id: 7,
                event: 'test',
                title: 'Test notification',
                message: '',
                level: 'normal',
                ageSeconds: 1,
                channels: { pushover: { status, detail } },
              },
            ];
      return HttpResponse.json({ enabled: true, alerts });
    }),
  );
};

describe('Alerts - Send test', () => {
  it('reports the real delivery result, not just that it was queued', async () => {
    recentAfterTest('failed', 'HTTP 400: user identifier is not a valid user');
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    fireEvent.click((await screen.findAllByText('Send test'))[0]);
    expect(await screen.findByText('Sending...')).toBeInTheDocument();
    expect(await screen.findByText('Failed: HTTP 400: user identifier is not a valid user', {}, { timeout: 4000 })).toBeInTheDocument();
  });

  it('says Delivered when the channel reports sent', async () => {
    recentAfterTest('sent', 'HTTP 200');
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    fireEvent.click((await screen.findAllByText('Send test'))[0]);
    expect(await screen.findByText('Delivered.', {}, { timeout: 4000 })).toBeInTheDocument();
  });
});
