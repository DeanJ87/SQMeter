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
      data: { firmware: { name: 'SQMeter', version: '0.1.3', buildDate: 'x', buildTime: 'y' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
  });

  it('shows the stable release list by default and flags it as newer', async () => {
    render(<Updates />);

    await waitFor(() => {
      expect(screen.getByRole('option', { name: /v0\.1\.4/ })).toBeInTheDocument();
    });

    expect(screen.getByText(/A newer release/)).toBeInTheDocument();
  });

  it('flags a newer beta for a beta build', async () => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.2.0-beta.1', buildDate: 'x', buildTime: 'y' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
    render(<Updates />);
    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.1\.4/ })).toBeInTheDocument());
    // The stable release is older than this beta.
    expect(screen.queryByText(/A newer release/)).toBeNull();
    fireEvent.change(screen.getByLabelText('Release track'), { target: { value: 'beta' } });
    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.2\.0-beta\.2/ })).toBeInTheDocument());
    expect(screen.getByText(/A newer release/)).toBeInTheDocument();
  });

  it('marks the installed release and will not reinstall it', async () => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.1.4', buildDate: 'x', buildTime: 'y' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
    render(<Updates />);
    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.1\.4.*installed/ })).toBeInTheDocument());
    expect(screen.getByRole('button', { name: 'Installed' })).toBeDisabled();
  });

  it('warns before a downgrade', async () => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.2.0-beta.3', buildDate: 'x', buildTime: 'y' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
    render(<Updates />);
    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.1\.4.*older/ })).toBeInTheDocument());
    expect(screen.getByText(/Older than v0\.2\.0-beta\.3/)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /Downgrade to v0\.1\.4/ })).toBeEnabled();
  });

  it('tells a device on the old partition layout it needs the USB flash (spec 027)', async () => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.3.0-beta.1', buildDate: 'x', buildTime: 'y', layout: 'legacy' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
    render(<Updates />);
    expect(screen.getByText(/needs the one-time USB flash/)).toBeInTheDocument();
    expect(screen.getByRole('link', { name: 'How to' })).toHaveAttribute('href', 'https://sqmeter.dev/getting-started/usb-flash/');
  });

  it('says nothing about the USB flash on the current layout', async () => {
    vi.mocked(useWebSocket).mockReturnValue({
      data: { firmware: { name: 'SQMeter', version: '0.3.0-beta.1', buildDate: 'x', buildTime: 'y', layout: 'l2' } },
      connected: true,
      lastMessageAt: Date.now(),
    });
    render(<Updates />);
    expect(screen.queryByText(/needs the one-time USB flash/)).toBeNull();
  });

  it('switches to the beta track and fetches beta releases', async () => {
    render(<Updates />);

    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.1\.4/ })).toBeInTheDocument());

    fireEvent.change(screen.getByLabelText('Release track'), { target: { value: 'beta' } });

    await waitFor(() => {
      expect(screen.getByRole('option', { name: /v0\.2\.0-beta\.2/ })).toBeInTheDocument();
    });
  });

  it('shows an error if the release check fails', async () => {
    server.use(http.get('/api/updates/check', () => HttpResponse.json({ error: 'GitHub API request failed (HTTP 503)' }, { status: 502 })));

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
      }),
    );

    render(<Updates />);

    await waitFor(() => expect(screen.getByRole('option', { name: /v0\.1\.4/ })).toBeInTheDocument());

    fireEvent.click(screen.getByRole('button', { name: /Update to v0\.1\.4/ }));

    await waitFor(() => expect(applyCalled).toBe(true));
  });
});
