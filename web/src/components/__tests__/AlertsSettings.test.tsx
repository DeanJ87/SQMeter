import { describe, it, expect, vi } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import AlertsSettings, { mergeAlertsConfig } from '../AlertsSettings';
import { mockConfig } from '../../mocks/data';

describe('AlertsSettings', () => {
  it('lists recent alerts with per-channel delivery status', async () => {
    render(<AlertsSettings config={mockConfig} updateConfig={vi.fn()} validationErrors={{}} />);

    expect(await screen.findByText('Observatory UNSAFE')).toBeInTheDocument();
    expect(screen.getAllByText('pushover: sent').length).toBeGreaterThan(0);
  });

  it('updates nested channel settings through updateConfig', () => {
    const updateConfig = vi.fn();
    render(<AlertsSettings config={mockConfig} updateConfig={updateConfig} validationErrors={{}} />);

    fireEvent.click(screen.getByLabelText('ntfy'));
    expect(updateConfig).toHaveBeenCalledWith(['alerts', 'ntfy', 'enabled'], true);
  });

  it('queues a test notification', async () => {
    render(<AlertsSettings config={mockConfig} updateConfig={vi.fn()} validationErrors={{}} />);

    fireEvent.click(screen.getByText('Send test'));
    expect(await screen.findByText(/Test queued/)).toBeInTheDocument();
  });

  it('fills in defaults for configs from older firmware', () => {
    const merged = mergeAlertsConfig({ enabled: true });
    expect(merged.enabled).toBe(true);
    expect(merged.ntfy.server).toBe('https://ntfy.sh');
    expect(merged.pushover.highPriority).toBe(1);
  });
});
