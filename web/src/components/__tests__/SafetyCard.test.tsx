import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/preact';
import SafetyCard from '../SafetyCard';
import type { SafetyStatus } from '../../types';

const base: SafetyStatus = {
  safe: true,
  rawSafe: true,
  alpacaEnabled: true,
  reasonFlags: 0,
  reasons: [],
  secondsUntilSafe: 0,
  evaluatedAgeMs: 100,
  changedAgeMs: 120000,
};

describe('SafetyCard', () => {
  it('shows Safe when all rules pass', () => {
    render(<SafetyCard safety={base} />);
    expect(screen.getByText('Safe')).toBeInTheDocument();
    expect(screen.getByText('All rules pass.')).toBeInTheDocument();
  });

  it('lists unsafe reasons such as rain', () => {
    render(<SafetyCard safety={{ ...base, safe: false, rawSafe: false, reasonFlags: 512, reasons: ['Rain detected'] }} />);
    expect(screen.getByText('Unsafe')).toBeInTheDocument();
    expect(screen.getByText(/Rain detected/)).toBeInTheDocument();
  });

  it('shows the countdown while the safe delay holds', () => {
    render(<SafetyCard safety={{ ...base, safe: false, rawSafe: true, secondsUntilSafe: 42 }} />);
    expect(screen.getByText('Safe in 42s')).toBeInTheDocument();
  });

  // Alpaca off is the Status card's Imaging app tile, and rules not in effect are
  // in Settings → Safety: the card shows only the verdict (DS-08).
  it('shows only the verdict, not Alpaca or rules not in effect', () => {
    render(<SafetyCard safety={{ ...base, alpacaEnabled: false, rulesNotInEffect: ['Unsafe while raining'] }} />);
    expect(screen.queryByText('Imaging apps')).toBeNull();
    expect(screen.queryByText('Unsafe while raining')).toBeNull();
    expect(screen.queryByText('Not in effect')).toBeNull();
  });
});
