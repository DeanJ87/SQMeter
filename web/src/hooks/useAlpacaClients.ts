import { useEffect, useState } from 'preact/hooks';
import type { SystemStatus } from '../types';

export type AlpacaClients = NonNullable<SystemStatus['alpaca']>['clients'];

// Whether an imaging app is checking each Alpaca device (specs/021), from
// /api/status; null until known, and from firmware that doesn't report it.
export const useAlpacaClients = (intervalMs: number) => {
  const [clients, setClients] = useState<AlpacaClients | null>(null);
  useEffect(() => {
    const poll = () =>
      fetch('/api/status')
        .then((response) => (response.ok ? response.json() : null))
        .then((data: SystemStatus | null) => setClients(data?.alpaca?.clients ?? null))
        .catch(() => setClients(null));
    poll();
    const timer = setInterval(poll, intervalMs);
    return () => clearInterval(timer);
  }, [intervalMs]);
  return clients;
};
