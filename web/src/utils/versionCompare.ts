/**
 * Compares a running firmware version (e.g. "0.0.2") against a GitHub
 * release tag (e.g. "v0.0.3") and returns true if the tag is newer.
 * Malformed input is treated as not-stale (fails safe: no update nagging).
 */
export const isVersionStale = (currentVersion: string, latestTag: string): boolean => {
  const parse = (v: string): number[] | null => {
    const cleaned = v.trim().replace(/^v/i, '');
    if (!/^\d+(\.\d+)*$/.test(cleaned)) return null;
    return cleaned.split('.').map(Number);
  };

  const current = parse(currentVersion);
  const latest = parse(latestTag);
  if (!current || !latest) return false;

  const length = Math.max(current.length, latest.length);
  for (let i = 0; i < length; i++) {
    const c = current[i] ?? 0;
    const l = latest[i] ?? 0;
    if (l > c) return true;
    if (l < c) return false;
  }
  return false;
};
