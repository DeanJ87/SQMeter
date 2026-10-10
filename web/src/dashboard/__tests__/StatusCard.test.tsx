import { describe, expect, it, vi } from 'vitest';
import { fireEvent, render, screen, within } from '@testing-library/preact';
import StatusCard from '../StatusCard';
import type { GlanceItem } from '../glance';

// The Status card as rendered (specs/026 FR-001..FR-003, DS-01..DS-08).

const item = (id: string, label: string, state: string, extra: Partial<GlanceItem> = {}): GlanceItem => ({
  id,
  severity: 'ok',
  priority: 1,
  label,
  state,
  ...extra,
});

const healthy = [
  item('imaging-app', 'Imaging app', 'Connected', { sub: 'Safety monitor' }),
  item('imaging-app', 'Imaging app', 'Waiting', { severity: 'idle', sub: 'Weather device' }),
  item('alerts-state', 'Alerts', 'Sending'),
];

describe('StatusCard', () => {
  it('is a card with tiles and an "All good" pill when nothing is wrong', () => {
    render(<StatusCard items={healthy} onAction={() => undefined} />);
    expect(screen.getByRole('heading', { name: 'Status' })).toBeTruthy();
    expect(screen.getByText('All good')).toBeTruthy();
    // A resting imaging app (nothing connected, alerts don't need one) isn't counted.
    expect(screen.getAllByText('Imaging app')).toHaveLength(2);
    const alerts = screen.getByText('Alerts').closest('.status-tile') as HTMLElement;
    expect(within(alerts).getByText('Sending')).toBeTruthy();
    expect(screen.getByText('Waiting').className).toContain('pill-dim');
    expect(screen.queryByRole('listitem')).toBeNull();
  });

  it('lists each problem once: name, pill, "?" - the row links to the fix', () => {
    const items = [
      ...healthy,
      item('no-channel', 'Alert channels', "Can't send", {
        severity: 'problem',
        detail: 'MQTT is off',
        fix: { label: 'Open settings', href: '/settings?tab=alerts' },
      }),
      item('clock-location', 'Clock', 'Not set', { severity: 'note', fix: { label: 'Open settings', href: '#/settings' } }),
    ];
    render(<StatusCard items={items} onAction={() => undefined} />);
    expect(screen.getByText('2 to check')).toBeTruthy();
    const row = screen.getByText('Alert channels').closest('li') as HTMLElement;
    expect(within(row).getByText("Can't send")).toBeTruthy();
    expect(within(row).getByRole('link').getAttribute('href')).toBe('/settings?tab=alerts');
    // One link per row, no repeated "Open settings" text on the page.
    expect(screen.queryAllByText('Open settings')).toHaveLength(0);
    // The detail is behind "?", not a paragraph.
    expect(within(row).getByRole('button', { name: /More information/i })).toBeTruthy();
  });

  // inventory: phone-alarm - the demo is the standard build, so the ringing
  // alarm (Bluetooth build) is checked here: a row with Acknowledge, which acts.
  it('shows a ringing phone alarm with Acknowledge, and hides it otherwise', () => {
    const onAction = vi.fn();
    const alarm = item('phone-alarm', 'Phone alarm', 'Ringing', {
      severity: 'problem',
      fix: { label: 'Acknowledge', action: 'acknowledge' },
    });
    const { unmount } = render(<StatusCard items={[...healthy, alarm]} onAction={onAction} />);
    expect(screen.getByText('Phone alarm').closest('[data-inventory="phone-alarm"]')).toBeTruthy();
    fireEvent.click(screen.getByRole('button', { name: 'Acknowledge' }));
    expect(onAction).toHaveBeenCalledWith('acknowledge');
    unmount();
    render(<StatusCard items={healthy} onAction={onAction} />);
    expect(document.querySelector('[data-inventory="phone-alarm"]')).toBeNull();
  });

  it('puts the paused alerts Resume action on the Alerts tile', () => {
    const onAction = vi.fn();
    const paused = item('alerts-state', 'Alerts', 'Paused', {
      severity: 'problem',
      sub: 'Paused by you.',
      fix: { label: 'Resume', action: 'resume' },
    });
    render(<StatusCard items={[healthy[0], healthy[1], paused]} onAction={onAction} />);
    const tile = screen.getByText('Alerts').closest('.status-tile') as HTMLElement;
    fireEvent.click(within(tile).getByRole('button', { name: 'Resume' }));
    expect(onAction).toHaveBeenCalledWith('resume');
  });
});
