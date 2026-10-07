import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen } from '@testing-library/preact';
import SunMoonCard from '../SunMoonCard';

describe('SunMoonCard', () => {
  afterEach(() => vi.useRealTimers());

  it('shows twilight, moon phase and the next rise/set times', () => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date('2026-03-03T23:00:00Z')); // full moon, winter-ish night
    render(<SunMoonCard latitude={51.4779} longitude={-0.0015} />);
    expect(screen.getByText('Astronomical dark')).toBeInTheDocument();
    expect(screen.getByText(/Full moon/)).toBeInTheDocument();
    expect(screen.getByText('Sunrise')).toBeInTheDocument();
    expect(screen.getByText('Moonset')).toBeInTheDocument();
  });
});
