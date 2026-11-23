import { afterEach, describe, expect, it } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig } from '../../mocks/data';
import { mockDevice } from '../../test/mockDevice';
import { server } from '../../test/mswServer';

// Share passwords (specs/037-share-secrets): the Security card toggle, its
// warning and its dependency on the device password (D-53).

const protectedAuth = { enabled: true, username: 'admin', password: '********' };
const WARNING = 'Anyone with the device password can read them over plain HTTP.';

describe('Share passwords', () => {
  afterEach(() => {
    window.history.replaceState(null, '', '/');
  });

  it('is off by default and disabled without a device password (D-53)', async () => {
    mockDevice();
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Share passwords');
    await waitFor(() => expect(toggle).toBeDisabled());
    expect(toggle).not.toBeChecked();
    expect(screen.getAllByText(/Set a device password/).length).toBeGreaterThan(0);
    expect(screen.queryByText(WARNING)).toBeNull();
  });

  it('turns on with a device password, warns, and is saved', async () => {
    let saved: Record<string, unknown> | null = null;
    mockDevice({ config: { auth: protectedAuth } });
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = (await request.json()) as Record<string, unknown>;
        return HttpResponse.json({ success: true });
      }),
    );
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Share passwords');
    await waitFor(() => expect(toggle).not.toBeDisabled());
    expect(screen.queryByText(WARNING)).toBeNull();

    fireEvent.click(toggle);
    expect(toggle).toBeChecked();
    expect(await screen.findByText(WARNING)).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved!.sharing).toEqual({ secrets: true });
  });

  it('says it is not in effect when sharing is on but password protection is off', async () => {
    mockDevice({ config: { sharing: { secrets: true } } });
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Share passwords');
    expect(toggle).toBeChecked();
    await waitFor(() => expect(screen.getAllByText(/Set a device password/).length).toBeGreaterThan(0));
    expect(screen.queryByText(WARNING)).toBeNull();
  });

  it('is not shown for firmware without the setting', async () => {
    const { sharing: _sharing, ...older } = mockConfig;
    server.use(http.get('/api/config', () => HttpResponse.json(older)));
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);

    expect(await screen.findByLabelText('Password-protect changes')).toBeInTheDocument();
    expect(screen.queryByLabelText('Share passwords')).toBeNull();
  });
});
