import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, fireEvent } from '@testing-library/preact';
import { http, HttpResponse } from 'msw';
import Updates from '../Updates';
import { server } from '../../test/mswServer';
import { mockGithubReleases } from '../../mocks/data';

vi.mock('../../hooks/useWebSocket', () => ({
  useWebSocket: vi.fn(),
}));

import { useWebSocket } from '../../hooks/useWebSocket';

describe('Updates - GitHub check for updates', () => {
  beforeEach(() => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.0.1', buildDate: 'x', buildTime: 'y' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
  });

  it('shows the stable release list by default and flags it as newer', async () => {
    render(<Updates />);

    await waitFor(() => {
      expect(screen.getByRole('option', { name: /v0\.0\.3/ })).toBeInTheDocument();
    });

    expect(screen.getByText(/A newer release/)).toBeInTheDocument();
  });

  it('switches to the beta track and fetches beta releases', async () => {
    render(<Updates />);

    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.0\.3/ })).toBeInTheDocument());

    fireEvent.change(screen.getByLabelText('Release track'), { target: { value: 'beta' } });

    await waitFor(() => {
      expect(screen.getByRole('option', { name: /v0\.0\.4-beta\.1/ })).toBeInTheDocument();
    });
  });

  it('shows an error if the release check fails', async () => {
    server.use(
      http.get('/api/updates/check', () =>
        HttpResponse.json({ error: 'GitHub API request failed (HTTP 503)' }, { status: 502 })
      )
    );

    render(<Updates />);

    await waitFor(() => {
      expect(screen.getByText(/Failed to check for updates/)).toBeInTheDocument();
    });
  });

  it('starts an update when the apply button is clicked', async () => {
    let applyCalled = false;
    server.use(
      http.post('/api/updates/apply', async ({ request }) => {
        const body = await request.json();
        applyCalled = true;
        expect(body).toMatchObject({
          firmwareAssetUrl: mockGithubReleases[0].firmwareAssetUrl,
          fsAssetUrl: mockGithubReleases[0].fsAssetUrl,
        });
        return HttpResponse.json({ success: true, message: 'Update started' });
      })
    );

    render(<Updates />);

    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.0\.3/ })).toBeInTheDocument());

    fireEvent.click(screen.getByRole('button', { name: /Update to v0\.0\.3/ }));

    await waitFor(() => expect(applyCalled).toBe(true));
  });
});
