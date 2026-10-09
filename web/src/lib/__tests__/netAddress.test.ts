import { describe, expect, it } from 'vitest';
import {
  allowedPeer,
  formatIpv6,
  hostForUrl,
  isIpv4Mapped,
  isIpv6Literal,
  parseHost,
  parseHttpUrl,
  parseIpv6,
  scopeOf,
  type Ipv6,
} from '../netAddress';
import cases from '../../../../test/fixtures/net-address/cases.json';

// The device (lib/NetAddress) and this mirror must give the same answers:
// both run test/fixtures/net-address/cases.json (specs/015-ipv6-dual-stack).

const address = (text: string): Ipv6 => {
  const parsed = parseIpv6(text);
  if (!parsed) throw new Error(`fixture address ${text} doesn't parse`);
  return parsed;
};

describe('IPv6 addresses', () => {
  it.each(cases.addresses)('$in', (c) => {
    const parsed = parseIpv6(c.in);
    if ('invalid' in c && c.invalid) {
      expect(parsed).toBeNull();
      return;
    }
    expect(parsed).not.toBeNull();
    expect(formatIpv6(parsed as Ipv6)).toBe(c.out);
    expect(scopeOf(parsed as Ipv6)).toBe(c.scope);
    expect(isIpv4Mapped(parsed as Ipv6)).toBe('mapped' in c ? c.mapped : false);
  });
});

describe('IPv6 peers', () => {
  it.each(cases.peers)('$peer', (c) => {
    expect(allowedPeer(address(c.peer), c.own.map(address))).toBe(c.allowed);
  });
});

describe('hosts', () => {
  it.each(cases.hosts)('$in', (c) => {
    const result = parseHost(c.in);
    if ('error' in c) {
      expect(result.error).toBe(c.error);
      return;
    }
    expect(result.host).toEqual({ name: c.name, ipv6: c.ipv6, port: c.port });
  });
});

describe('http URLs', () => {
  it.each(cases.urls)('$in', (c) => {
    const result = parseHttpUrl(c.in);
    if ('error' in c) {
      expect(result.error).toBe(c.error);
      return;
    }
    expect(result.url?.https).toBe(c.https);
    expect(result.url?.host.name).toBe(c.host);
    expect(result.url?.host.ipv6).toBe(c.ipv6);
    expect(result.url?.port).toBe(c.port);
    expect(result.url?.path).toBe(c.path);
  });

  it('spots IPv6 literals (NTP servers refuse them)', () => {
    expect(isIpv6Literal('2001:db8::123')).toBe(true);
    expect(isIpv6Literal('[fd00::1]')).toBe(true);
    expect(isIpv6Literal('pool.ntp.org')).toBe(false);
    expect(isIpv6Literal('192.168.1.1')).toBe(false);
  });

  it('brackets IPv6 hosts only', () => {
    expect(hostForUrl({ name: 'fd00::10', ipv6: true, port: 0 })).toBe('[fd00::10]');
    expect(hostForUrl({ name: 'broker.local', ipv6: false, port: 0 })).toBe('broker.local');
  });
});
