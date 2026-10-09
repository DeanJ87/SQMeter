import { afterEach, describe, expect, it, vi } from 'vitest';
import { act, renderHook } from '@testing-library/preact';
import { QUIET_AFTER_MS, useQuiet } from '../../hooks/useQuiet';

// A socket that stays open but stops sending turns the dashboard Stale
// (spec 010 US1-AS2).
describe('useQuiet', () => {
  afterEach(() => vi.useRealTimers());

  it('is not quiet before the first message', () => {
    const { result } = renderHook(() => useQuiet(null));
    expect(result.current).toBe(false);
  });

  it('turns quiet once nothing has arrived for the threshold, and back on a new message', () => {
    vi.useFakeTimers();
    const start = Date.now();
    const { result, rerender } = renderHook(({ at }) => useQuiet(at), { initialProps: { at: start as number | null } });
    expect(result.current).toBe(false);
    act(() => {
      vi.advanceTimersByTime(QUIET_AFTER_MS - 1000);
    });
    expect(result.current).toBe(false);
    act(() => {
      vi.advanceTimersByTime(2000);
    });
    expect(result.current).toBe(true);
    rerender({ at: Date.now() });
    expect(result.current).toBe(false);
  });
});
