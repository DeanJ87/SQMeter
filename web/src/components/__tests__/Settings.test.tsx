import { describe, it, expect, vi, afterEach } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
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
    expect(await screen.findByText('Observatory UNSAFE')).toBeInTheDocument();
  });

  it('greys out rain rules when the rain sensor is off, with a link to set it up', async () => {
    withConfig({ rain: { ...mockConfig.rain, enabled: false }, alpaca: { ...mockConfig.alpaca, rainUnsafeEnabled: false } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = (await screen.findByText('Unsafe while raining')).closest('label')!.querySelector('input')!;
    expect(toggle).toBeDisabled();
    expect(screen.getAllByText('The rain sensor is turned off.').length).toBeGreaterThan(0);

    fireEvent.click(screen.getAllByText(/Set up the rain sensor/)[0]);
    expect(await screen.findByRole('tab', { name: 'Sensors', selected: true })).toBeInTheDocument();
  });

  it('lets a rule that is already on be switched off even without its sensor', async () => {
    withConfig({ rain: { ...mockConfig.rain, enabled: false } }); // rainUnsafeEnabled defaults on
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = (await screen.findByText('Unsafe while raining')).closest('label')!.querySelector('input')!;
    expect(toggle).not.toBeDisabled();
    expect(screen.getAllByText(/While this rule is on, the SafetyMonitor reports unsafe/).length).toBe(2);
  });

  it('greys out sky rules when the MLX90614 was not detected', async () => {
    server.use(
      http.get('/api/status', () =>
        HttpResponse.json({ ...mockStatus, sensors: { ...mockStatus.sensors, mlx90614: { initialized: false, status: 1, lastUpdate: 0 } } })
      )
    );
    withConfig({ alpaca: { ...mockConfig.alpaca, cloudCoverEnabled: false } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    await waitFor(() => {
      const toggle = screen.getByText('Maximum cloud cover').closest('label')!.querySelector('input')!;
      expect(toggle).toBeDisabled();
    });
  });

  it('tracks unsaved changes and blocks alert tests until saved', async () => {
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    expect(await screen.findByText('All changes saved')).toBeInTheDocument();
    const pushoverTest = () => screen.getAllByText('Send test')[0];
    expect(pushoverTest()).not.toBeDisabled();

    fireEvent.click(screen.getByLabelText('ntfy'));
    expect(screen.getByText('Unsaved changes')).toBeInTheDocument();
    expect(pushoverTest()).toBeDisabled();

    fireEvent.click(screen.getByText('Discard'));
    expect(screen.getByText('All changes saved')).toBeInTheDocument();
  });

  it('jumps to the tab holding the first validation error on save', async () => {
    window.history.replaceState(null, '', '/settings?tab=device');
    withConfig({ alerts: { ...mockConfig.alerts, ntfy: { enabled: true, server: 'https://ntfy.sh', topic: 'x', token: '' } } });
    render(<Settings />);
    fireEvent.click(await screen.findByRole('tab', { name: 'Alerts' }));
    fireEvent.input(screen.getByDisplayValue('x'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('tab', { name: 'Device' }));
    fireEvent.click(screen.getByText('Save settings'));

    expect(await screen.findByRole('tab', { name: 'Alerts', selected: true })).toBeInTheDocument();
    expect(screen.getByText('Topic is required')).toBeInTheDocument();
  });

  it('explains Bluetooth needs the BLE build on standard firmware', async () => {
    render(<Settings />);
    expect(await screen.findByText(/Bluetooth isn't in this firmware/)).toBeInTheDocument();
  });
});
