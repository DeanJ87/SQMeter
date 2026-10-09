import { beforeEach, describe, expect, it } from 'vitest';
import { TOUR_STORAGE_KEY, Tour } from '../tour/tourState';

const memoryStore = () => {
  const values = new Map<string, string>();
  return {
    getItem: (key: string) => values.get(key) ?? null,
    setItem: (key: string, value: string) => void values.set(key, value),
    values,
  };
};

let storage = memoryStore();

describe('Tour', () => {
  beforeEach(() => {
    storage = memoryStore();
  });

  it('is offered on a first visit', () => {
    expect(new Tour(storage).offered).toBe(true);
  });

  it.each(['done', 'dismissed'] as const)('is not offered again once %s', (outcome) => {
    const first = new Tour(storage);
    first.start();
    first.end(outcome);
    expect(storage.getItem(TOUR_STORAGE_KEY)).toBe(outcome);
    expect(new Tour(storage).offered).toBe(false);
  });

  it('starts at the first step and moves between steps', () => {
    const tour = new Tour(storage);
    tour.start();
    expect(tour.step).toBe(0);
    expect(tour.offered).toBe(false);
    tour.go(3);
    expect(tour.step).toBe(3);
    tour.end('dismissed');
    expect(tour.step).toBeNull();
  });

  it('offers the tour when storage is unavailable', () => {
    expect(new Tour(null).offered).toBe(true);
  });
});
