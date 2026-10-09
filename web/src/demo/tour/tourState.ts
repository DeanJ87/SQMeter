// The demo tour's state (specs/018 US1): which step is showing, and whether
// this browser has already been offered the tour (a per-viewer convenience in
// localStorage; if storage is unavailable the offer simply comes back).

export const TOUR_STORAGE_KEY = 'sqm.demo.tour.v1';

export type TourOutcome = 'done' | 'dismissed';

type Listener = () => void;

export interface FlagStore {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}

const browserStorage = (): FlagStore | null => {
  try {
    return localStorage;
  } catch {
    return null;
  }
};

export class Tour {
  /** Index of the showing step, or null when the tour isn't running. */
  step: number | null = null;
  private offerOpen: boolean;
  private listeners = new Set<Listener>();

  constructor(private storage: FlagStore | null = browserStorage()) {
    this.offerOpen = this.readFlag() === null;
  }

  /** The first-visit offer is showing. */
  get offered() {
    return this.offerOpen && this.step === null;
  }

  start() {
    this.offerOpen = false;
    this.step = 0;
    this.changed();
  }

  go(index: number) {
    this.step = index;
    this.changed();
  }

  /** Finished or skipped: not offered again in this browser. */
  end(outcome: TourOutcome) {
    this.step = null;
    this.offerOpen = false;
    this.writeFlag(outcome);
    this.changed();
  }

  onChange(listener: Listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private readFlag(): string | null {
    try {
      return this.storage?.getItem(TOUR_STORAGE_KEY) ?? null;
    } catch {
      return null;
    }
  }

  private writeFlag(outcome: TourOutcome) {
    try {
      this.storage?.setItem(TOUR_STORAGE_KEY, outcome);
    } catch {
      // private mode: the tour is offered again next visit
    }
  }

  private changed() {
    this.listeners.forEach((listener) => listener());
  }
}

export const tour = new Tour();
