// IPv6 addresses, hosts and URLs as settings accept them
// (specs/015-ipv6-dual-stack). Mirrors lib/NetAddress on the device; both run
// test/fixtures/net-address/cases.json, so the browser and the device agree.

export type Ipv6 = number[]; // 16 bytes

export type Ipv6Scope = 'unspecified' | 'loopback' | 'multicast' | 'link-local' | 'unique-local' | 'global';

const parseIpv4 = (text: string): number[] | null => {
  const parts = text.split('.');
  if (parts.length !== 4) return null;
  const bytes = parts.map((part) => (/^\d{1,3}$/.test(part) ? Number(part) : NaN));
  return bytes.every((b) => b <= 255) ? bytes : null;
};

const parseGroups = (text: string, allowIpv4Tail: boolean): number[] | null => {
  if (text === '') return [];
  const groups: number[] = [];
  const parts = text.split(':');
  for (const [i, part] of parts.entries()) {
    if (i === parts.length - 1 && allowIpv4Tail && part.includes('.')) {
      const v4 = parseIpv4(part);
      if (!v4) return null;
      groups.push((v4[0] << 8) | v4[1], (v4[2] << 8) | v4[3]);
      continue;
    }
    if (!/^[0-9a-fA-F]{1,4}$/.test(part)) return null;
    groups.push(parseInt(part, 16));
  }
  return groups;
};

/** Text form without brackets or a zone. */
export const parseIpv6 = (text: string): Ipv6 | null => {
  if (text === '' || text.length > 45) return null;
  const gap = text.indexOf('::');
  if (gap !== -1 && text.indexOf('::', gap + 1) !== -1) return null;
  let groups: number[];
  if (gap === -1) {
    const all = parseGroups(text, true);
    if (!all || all.length !== 8) return null;
    groups = all;
  } else {
    const head = parseGroups(text.slice(0, gap), false);
    const tail = parseGroups(text.slice(gap + 2), true);
    if (!head || !tail || head.length + tail.length > 7) return null;
    groups = [...head, ...Array<number>(8 - head.length - tail.length).fill(0), ...tail];
  }
  return groups.flatMap((g) => [g >> 8, g & 0xff]);
};

/** RFC 5952: lower case, longest run of 2+ zero groups as "::". */
export const formatIpv6 = (address: Ipv6): string => {
  const groups = Array.from({ length: 8 }, (_, i) => (address[i * 2] << 8) | address[i * 2 + 1]);
  let bestStart = -1;
  let bestLength = 1;
  for (let i = 0; i < 8; ) {
    if (groups[i] !== 0) {
      i++;
      continue;
    }
    let j = i;
    while (j < 8 && groups[j] === 0) j++;
    if (j - i > bestLength) {
      bestStart = i;
      bestLength = j - i;
    }
    i = j;
  }
  if (bestStart === -1) return groups.map((g) => g.toString(16)).join(':');
  const before = groups.slice(0, bestStart).map((g) => g.toString(16));
  const after = groups.slice(bestStart + bestLength).map((g) => g.toString(16));
  return `${before.join(':')}::${after.join(':')}`;
};

export const isIpv4Mapped = (a: Ipv6) => a.slice(0, 10).every((b) => b === 0) && a[10] === 0xff && a[11] === 0xff;

export const scopeOf = (a: Ipv6): Ipv6Scope => {
  const zero = a.slice(0, 15).every((b) => b === 0);
  if (zero && a[15] === 0) return 'unspecified';
  if (zero && a[15] === 1) return 'loopback';
  if (a[0] === 0xff) return 'multicast';
  if (a[0] === 0xfe && (a[1] & 0xc0) === 0x80) return 'link-local';
  if ((a[0] & 0xfe) === 0xfc) return 'unique-local';
  return 'global';
};

/** FR-011: the IPv6 peers the device accepts. */
export const allowedPeer = (peer: Ipv6, own: Ipv6[]): boolean => {
  if (isIpv4Mapped(peer)) return true;
  const scope = scopeOf(peer);
  if (scope === 'loopback' || scope === 'link-local') return true;
  if (scope !== 'unique-local' && scope !== 'global') return false;
  return own.some((mine) => {
    const s = scopeOf(mine);
    return (s === 'unique-local' || s === 'global') && mine.slice(0, 8).every((b, i) => b === peer[i]);
  });
};

export interface Host {
  name: string;
  ipv6: boolean;
  port: number; // 0: none given
}

export const HOST_ERRORS = {
  empty: 'Enter a host name or address',
  badIpv6: 'Not a valid IPv6 address',
  needsBrackets: 'Put IPv6 addresses in brackets to add a port, e.g. [fd00::10]:1883',
  portInField: 'Put the port in the Port field',
  hasZone: 'Leave out the %zone - the device has one network interface',
  badPort: 'Port must be 1-65535',
  spaces: "Host can't contain spaces",
} as const;

export type HostResult = { host: Host; error?: undefined } | { host?: undefined; error: string };

const parsePort = (text: string): number | null => {
  if (!/^\d{1,5}$/.test(text)) return null;
  const port = Number(text);
  return port >= 1 && port <= 65535 ? port : null;
};

const parseBracketed = (text: string): HostResult => {
  const close = text.indexOf(']');
  if (close === -1) return { error: HOST_ERRORS.badIpv6 };
  const inside = text.slice(1, close);
  if (inside.includes('%')) return { error: HOST_ERRORS.hasZone };
  if (!parseIpv6(inside)) return { error: HOST_ERRORS.badIpv6 };
  const rest = text.slice(close + 1);
  let port = 0;
  if (rest !== '') {
    const parsed = rest.startsWith(':') ? parsePort(rest.slice(1)) : null;
    if (parsed === null) return { error: HOST_ERRORS.badPort };
    port = parsed;
  }
  return { host: { name: inside, ipv6: true, port } };
};

/** A host as typed in settings: name, IPv4, IPv6 bare or bracketed, optional `[v6]:port`. */
export const parseHost = (text: string): HostResult => {
  if (text === '') return { error: HOST_ERRORS.empty };
  if (/\s/.test(text)) return { error: HOST_ERRORS.spaces };
  if (text.startsWith('[')) return parseBracketed(text);
  if (text.includes('%')) return { error: HOST_ERRORS.hasZone };
  const colons = text.split(':').length - 1;
  if (colons === 0) return { host: { name: text, ipv6: false, port: 0 } };
  if (colons === 1) return { error: HOST_ERRORS.portInField };
  if (parseIpv6(text)) return { host: { name: text, ipv6: true, port: 0 } };
  const last = text.lastIndexOf(':');
  if (parsePort(text.slice(last + 1)) !== null && parseIpv6(text.slice(0, last))) return { error: HOST_ERRORS.needsBrackets };
  return { error: HOST_ERRORS.badIpv6 };
};

export interface HttpUrl {
  https: boolean;
  host: Host;
  port: number;
  path: string;
}

export const URL_ERRORS = {
  scheme: 'URL must start with http:// or https://',
  httpsIpv6: "https to an IPv6 address isn't supported yet - use a host name, or http",
} as const;

export type UrlResult = { url: HttpUrl; error?: undefined } | { url?: undefined; error: string };

export const parseHttpUrl = (text: string): UrlResult => {
  let https = false;
  let rest: string;
  if (text.startsWith('http://')) rest = text.slice(7);
  else if (text.startsWith('https://')) {
    https = true;
    rest = text.slice(8);
  } else return { error: URL_ERRORS.scheme };

  const end = rest.search(/[/?#]/);
  const authority = end === -1 ? rest : rest.slice(0, end);
  let path = end === -1 ? '/' : rest.slice(end);
  if (!path.startsWith('/')) path = `/${path}`;

  let result: HostResult;
  if (authority.startsWith('[')) {
    result = parseHost(authority);
  } else {
    const colon = authority.indexOf(':');
    const name = colon === -1 ? authority : authority.slice(0, colon);
    if (colon !== -1 && authority.indexOf(':', colon + 1) !== -1) result = { error: HOST_ERRORS.needsBrackets };
    else {
      result = parseHost(name);
      if (result.host && colon !== -1) {
        const port = parsePort(authority.slice(colon + 1));
        result = port === null ? { error: HOST_ERRORS.badPort } : { host: { ...result.host, port } };
      }
    }
  }
  if (!result.host) return { error: result.error };
  if (https && result.host.ipv6) return { error: URL_ERRORS.httpsIpv6 };
  return { url: { https, host: result.host, port: result.host.port || (https ? 443 : 80), path } };
};

export const hostForUrl = (host: Host) => (host.ipv6 ? `[${host.name}]` : host.name);
