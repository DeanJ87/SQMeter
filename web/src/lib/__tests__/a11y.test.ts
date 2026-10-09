// @vitest-environment jsdom
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { announce, LIVE_REGION_ID, nextTabIndex, resetAnnouncements, summariseSeries } from '../a11y';

describe('announce', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    resetAnnouncements();
    document.body.innerHTML = `<div id="${LIVE_REGION_ID}" role="status" aria-live="polite"></div>`;
  });
  afterEach(() => vi.useRealTimers());

  it('writes the message to the live region', () => {
    expect(announce('Observatory unsafe', 1000)).toBe(true);
    vi.advanceTimersByTime(60);
    expect(document.getElementById(LIVE_REGION_ID)?.textContent).toBe('Observatory unsafe');
  });

  it('drops the same message within 2 s, but not after', () => {
    expect(announce('Reconnected', 1000)).toBe(true);
    expect(announce('Reconnected', 2500)).toBe(false);
    expect(announce('Reconnected', 3100)).toBe(true);
  });

  it('never announces empty text', () => {
    expect(announce('', 1000)).toBe(false);
  });
});

describe('nextTabIndex', () => {
  it.each([
    ['ArrowRight', 0, 1],
    ['ArrowRight', 5, 0],
    ['ArrowLeft', 0, 5],
    ['Home', 3, 0],
    ['End', 1, 5],
  ])('%s from %i goes to %i of 6', (key, index, expected) => {
    expect(nextTabIndex(key, index, 6)).toBe(expected);
  });

  it('ignores other keys', () => {
    expect(nextTabIndex('Enter', 2, 6)).toBeNull();
  });

  it('runs the other way right-to-left', () => {
    expect(nextTabIndex('ArrowLeft', 0, 6, true)).toBe(1);
    expect(nextTabIndex('ArrowRight', 0, 6, true)).toBe(5);
  });
});

describe('summariseSeries', () => {
  it('says there is no data with fewer than two readings', () => {
    expect(summariseSeries('SQM', [21])).toBe('SQM trend: no data yet');
  });

  it('describes a rising series with its range', () => {
    expect(summariseSeries('SQM', [20.9, 21.1, 21.4], (v) => v.toFixed(1))).toBe(
      'SQM trend: rising, 20.9 to 21.4 (range 20.9 to 21.4) over the last 3 readings',
    );
  });

  it('calls a flat series steady', () => {
    expect(summariseSeries('Cloud', [10, 12, 10])).toMatch(/^Cloud trend: steady/);
  });

  it('describes a falling series', () => {
    expect(summariseSeries('Temp', [12, 9, 6])).toMatch(/^Temp trend: falling/);
  });
});
