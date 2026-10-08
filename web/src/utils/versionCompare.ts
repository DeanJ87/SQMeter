type Version = { core: number[]; pre: string[] };

// "v0.2.0-beta.1+dev" -> core [0,2,0], pre ["beta","1"]; build metadata
// ("+dev", used by local builds) doesn't affect ordering.
const parse = (v: string): Version | null => {
  const match = v.trim().replace(/^v/i, '').match(/^(\d+(?:\.\d+)*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$/);
  if (!match) return null;
  return { core: match[1].split('.').map(Number), pre: match[2] ? match[2].split('.') : [] };
};

// Semantic-version precedence: a prerelease sorts before its release
// (0.2.0-beta.2 < 0.2.0), numeric identifiers compare as numbers.
export const compareVersions = (a: string, b: string): number | null => {
  const x = parse(a);
  const y = parse(b);
  if (!x || !y) return null;

  for (let i = 0; i < Math.max(x.core.length, y.core.length); i++) {
    const d = (x.core[i] ?? 0) - (y.core[i] ?? 0);
    if (d !== 0) return Math.sign(d);
  }
  if (!x.pre.length || !y.pre.length) return x.pre.length === y.pre.length ? 0 : x.pre.length ? -1 : 1;
  for (let i = 0; i < Math.max(x.pre.length, y.pre.length); i++) {
    const p = x.pre[i];
    const q = y.pre[i];
    if (p === undefined) return -1;
    if (q === undefined) return 1;
    const pn = /^\d+$/.test(p);
    const qn = /^\d+$/.test(q);
    if (pn && qn) {
      if (Number(p) !== Number(q)) return Math.sign(Number(p) - Number(q));
    } else if (pn !== qn) {
      return pn ? -1 : 1;
    } else if (p !== q) {
      return p < q ? -1 : 1;
    }
  }
  return 0;
};

/**
 * True if the release `latestTag` (e.g. "v0.2.0-beta.2") is newer than the
 * running firmware (e.g. "0.2.0-beta.1" or a local "0.2.0-beta.1+dev").
 * Malformed input is treated as not-stale (fails safe: no update nagging).
 */
export const isVersionStale = (currentVersion: string, latestTag: string): boolean =>
  (compareVersions(latestTag, currentVersion) ?? 0) > 0;
