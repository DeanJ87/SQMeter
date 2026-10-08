import { describe, it, expect, vi, afterEach, beforeAll } from 'vitest';
import { fireEvent, render, screen } from '@testing-library/preact';
import SunMoonCard from '../SunMoonCard';
import { nightWindow } from '../NightChart';

beforeAll(() => {
  if (!('ResizeObserver' in globalThis)) {
    (globalThis as any).ResizeObserver = class {
      observe() {}
      disconnect() {}
    };
  }
});

describe('SunMoonCard', () => {
  afterEach(() => vi.useRealTimers());

  it('summarises the moon and tonight, and charts noon to noon', () => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date('2026-03-03T23:00:00Z')); // full moon
    render(<SunMoonCard latitude={51.4779} longitude={-0.0015} />);
    expect(screen.getByText(/Full moon · 100% lit/)).toBeInTheDocument();
    expect(screen.getByText(/^Dark now, until/)).toBeInTheDocument();
    expect(screen.getByRole('img', { name: /^Night chart, .* Moon \d+% lit\.$/ })).toBeInTheDocument();
  });

  it('shows the values under the pointer', () => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date('2026-03-03T23:00:00Z'));
    render(<SunMoonCard latitude={51.4779} longitude={-0.0015} />);
    const chart = screen.getByRole('img', { name: /^Night chart, .* Moon \d+% lit\.$/ });
    chart.getBoundingClientRect = () => ({ left: 0, top: 0, width: 320, height: 170 }) as DOMRect;
    // Middle of the plot is around local midnight.
    fireEvent.pointerMove(chart, { clientX: 26 + (320 - 32) / 2 });
    expect(screen.getByText(/^Sun -\d/)).toBeInTheDocument();
    expect(screen.getByText(/^Moon \d+° · \d+% lit/)).toBeInTheDocument();
  });

  it('windows the chart on the night ahead, or the one in progress before noon', () => {
    const evening = nightWindow(new Date(2026, 2, 3, 20, 0));
    expect(evening.start.getDate()).toBe(3);
    const early = nightWindow(new Date(2026, 2, 4, 3, 0));
    expect(early.start.getDate()).toBe(3);
    expect(early.end.getDate()).toBe(4);
  });
});
