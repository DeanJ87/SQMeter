import { describe, it, expect, vi, afterEach } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { Toaster } from '../toast';
import { mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';

const withConfig = (overrides: Record<string, unknown>) =>
  server.use(http.get('/api/config', () => HttpResponse.json({ ...mockConfig, ...overrides })));

describe('Settings', () => {
  afterEach(() => {
    window.history.replaceState(null, '', '/');
  });

  it('opens the Safety tab from the Alpaca setup link and scrolls to Alpaca', async () => {
    const scrollIntoView = vi.fn();
    Element.prototype.scrollIntoView = scrollIntoView;
    window.history.replaceState(null, '', '/settings?section=alpaca');

    const { container } = render(<Settings />);

    expect(await screen.findByRole('tab', { name: 'Safety', selected: true })).toBeInTheDocument();
    await waitFor(() => expect(scrollIntoView).toHaveBeenCalled());
    expect(scrollIntoView.mock.contexts[0]).toBe(container.querySelector('#alpaca'));
  });

  it('switches tabs and records the tab in the URL', async () => {
    render(<Settings />);
    fireEvent.click(await screen.findByRole('tab', { name: 'Alerts' }));
    expect(screen.getByRole('tab', { name: 'Alerts', selected: true })).toBeInTheDocument();
    expect(window.location.search).toBe('?tab=alerts');
    expect(await screen.findByText('Notify me when')).toBeInTheDocument();
  });

  it('greys out rain rules when the rain sensor is off, with a link to set it up', async () => {
    withConfig({ rain: { ...mockConfig.rain, enabled: false }, alpaca: { ...mockConfig.alpaca, rainUnsafeEnabled: false } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Unsafe while raining');
    expect(toggle).toBeDisabled();
    expect(screen.getAllByText(/Rain sensor is off\./).length).toBeGreaterThan(0);

    fireEvent.click(screen.getAllByText('Set up')[0]);
    expect(await screen.findByRole('tab', { name: 'Sensors', selected: true })).toBeInTheDocument();
  });

  it('lets a rule that is already on be switched off even without its sensor', async () => {
    withConfig({ rain: { ...mockConfig.rain, enabled: false } }); // rainUnsafeEnabled defaults on
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Unsafe while raining');
    expect(toggle).not.toBeDisabled();
    expect(screen.getAllByText(/Reports unsafe while on/).length).toBe(2);
  });

  it('greys out sky rules when the MLX90614 was not detected', async () => {
    server.use(
      http.get('/api/status', () =>
        HttpResponse.json({ ...mockStatus, sensors: { ...mockStatus.sensors, infrared: { status: 'missing', ageMs: 0 } } }),
      ),
    );
    withConfig({ alpaca: { ...mockConfig.alpaca, cloudCoverEnabled: false } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    await waitFor(() => {
      expect(screen.getByLabelText('Max cloud cover', { selector: 'input[type=checkbox]' })).toBeDisabled();
    });
  });

  it('shows the save bar only with unsaved changes, and blocks alert tests until saved', async () => {
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    const pushoverTest = async () => (await screen.findAllByText('Send test'))[0];
    expect(await pushoverTest()).not.toBeDisabled();
    expect(screen.queryByText('Unsaved changes')).toBeNull();

    fireEvent.click(screen.getByLabelText('ntfy'));
    expect(screen.getByText('Unsaved changes')).toBeInTheDocument();
    expect(await pushoverTest()).toBeDisabled();

    fireEvent.click(screen.getByText('Discard'));
    expect(screen.queryByText('Unsaved changes')).toBeNull();
  });

  it('offers a restart when a saved change only applies after one', async () => {
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Toaster />);
    render(<Settings />);

    fireEvent.click(await screen.findByLabelText('Serve Alpaca devices'));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('Saved. Restart to apply Alpaca discovery.')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Restart' })).toBeInTheDocument();
  });

  it('just confirms saves that apply immediately', async () => {
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Toaster />);
    render(<Settings />);

    fireEvent.click(await screen.findByLabelText('ntfy'));
    fireEvent.input(screen.getByDisplayValue('https://ntfy.sh').closest('.card-group')!.querySelector('[name="alerts.ntfy.topic"]')!, {
      target: { value: 'my-topic' },
    });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('Saved.')).toBeInTheDocument();
  });

  it('jumps to the tab holding the first validation error on save', async () => {
    window.history.replaceState(null, '', '/settings?tab=device');
    withConfig({ alerts: { ...mockConfig.alerts, ntfy: { enabled: true, server: 'https://ntfy.sh', topic: 'x', token: '' } } });
    render(<Settings />);
    fireEvent.click(await screen.findByRole('tab', { name: 'Alerts' }));
    fireEvent.input(screen.getByDisplayValue('x'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('tab', { name: 'Device' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByRole('tab', { name: 'Alerts', selected: true })).toBeInTheDocument();
    expect(screen.getByText('Topic is required')).toBeInTheDocument();
  });

  it('explains Bluetooth needs the BLE build on standard firmware', async () => {
    render(<Settings />);
    expect(await screen.findByText(/Needs the Bluetooth firmware build/)).toBeInTheDocument();
  });
});
