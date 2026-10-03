import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/preact';
import Alpaca from '../Alpaca';

describe('Alpaca', () => {
  it('lists configured devices with their setup URLs and live device state', async () => {
    render(<Alpaca />);

    expect(await screen.findByText('SQMeter SafetyMonitor')).toBeInTheDocument();
    expect(screen.getByText('SQMeter ObservingConditions')).toBeInTheDocument();
    expect(
      screen.getByText(`${window.location.origin}/setup/v1/observingconditions/0/setup`)
    ).toBeInTheDocument();
    expect(await screen.findByText('IsSafe')).toBeInTheDocument();
    expect(await screen.findByText('CloudCover')).toBeInTheDocument();
  });
});
