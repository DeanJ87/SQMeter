import { afterEach, describe, expect, it } from 'vitest';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { RainCard } from '../dashboard/weatherCards';
import { generateSensorData, mockConfig } from '../../mocks/data';
import { mockDevice } from '../../test/mockDevice';

// Rain needs cloud (specs/036-rain-needs-cloud): the Safety rule row and the
// rain card's dew note.

const rain = generateSensorData().rain!;

describe('Rain needs cloud', () => {
  afterEach(() => {
    window.history.replaceState(null, '', '/');
  });

  it('shows the dew note on the rain card while dew is suspected', () => {
    const { rerender } = render(<RainCard rain={{ ...rain, raining: true }} config={mockConfig} dewSuspected />);
    expect(screen.getByText('Rain under a clear sky: dew suspected')).toBeInTheDocument();
    rerender(<RainCard rain={{ ...rain, raining: true }} config={mockConfig} />);
    expect(screen.queryByText('Rain under a clear sky: dew suspected')).toBeNull();
  });

  it('is a rule row in the Rain group, off with its level field inactive by default', async () => {
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: true } } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Rain needs cloud');
    await waitFor(() => expect(toggle).not.toBeDisabled());
    expect(toggle).not.toBeChecked();
    const level = screen.getByLabelText('Cloud to confirm rain', { selector: 'input' });
    expect(level).toBeDisabled();
    expect((level as HTMLInputElement).value).toBe('10');

    fireEvent.click(toggle);
    await waitFor(() => expect(screen.getByLabelText('Cloud to confirm rain', { selector: 'input' })).not.toBeDisabled());
  });

  it('is disabled while the rain sensor is off (D-52)', async () => {
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: false } } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Rain needs cloud');
    await waitFor(() => expect(toggle).toBeDisabled());
  });

  it('is disabled while the IR sky sensor is not detected (D-52)', async () => {
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: true } }, facts: { infraredDetected: false } });
    window.history.replaceState(null, '', '/settings?tab=safety');
    render(<Settings />);

    const toggle = await screen.findByLabelText('Rain needs cloud');
    await waitFor(() => expect(toggle).toBeDisabled());
    expect(screen.getAllByText(/IR sky sensor not detected\./).length).toBeGreaterThan(0);
  });
});
