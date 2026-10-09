import { useEffect, useState } from 'preact/hooks';

// The device pushes /ws/sensors every second. A socket that stays open but
// sends nothing for this long counts as stale (spec 010 US1-AS2).
export const QUIET_AFTER_MS = 5000;

/** True once no message has arrived for `afterMs`, re-checked every second. */
export const useQuiet = (lastMessageAt: number | null, afterMs = QUIET_AFTER_MS) => {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), 1000);
    return () => window.clearInterval(timer);
  }, []);
  return lastMessageAt !== null && now - lastMessageAt > afterMs;
};
