import type { SystemStatus } from '../types';

// The device's own date and time (from /api/status), so Sun & Moon and
// darkness describe the sky the device is under. Undefined until the device
// has a real clock (before NTP or GPS time it reports 1970).
const EARLIEST_VALID = Date.UTC(2024, 0, 1);

export const deviceTime = (status?: Pick<SystemStatus, 'time'> | null): Date | undefined => {
  const iso = status?.time?.iso;
  if (!iso) return undefined;
  const at = new Date(iso);
  return Number.isNaN(at.getTime()) || at.getTime() < EARLIEST_VALID ? undefined : at;
};
