import { describe, expect, it } from 'vitest';
import { connectPacket, disconnectPacket, publishPacket, remainingLength } from '../mqttPublish';

const bytes = (u: Uint8Array) => Array.from(u);

describe('MQTT packets', () => {
  it('encodes remaining length 7 bits per byte', () => {
    expect(bytes(remainingLength(0))).toEqual([0]);
    expect(bytes(remainingLength(127))).toEqual([127]);
    expect(bytes(remainingLength(128))).toEqual([0x80, 0x01]);
    expect(bytes(remainingLength(16_383))).toEqual([0xff, 0x7f]);
  });

  it('CONNECT: MQTT 3.1.1, clean session, username and password', () => {
    const p = connectPacket('c1', { username: 'u', password: 'p' });
    expect(p[0]).toBe(0x10);
    // header: "MQTT", level 4, flags, keepalive 30
    expect(bytes(p.slice(2, 12))).toEqual([0, 4, 77, 81, 84, 84, 4, 0xc2, 0, 30]);
    expect(bytes(p.slice(12))).toEqual([0, 2, 99, 49, 0, 1, 117, 0, 1, 112]);
    expect(p[1]).toBe(p.length - 2);
  });

  it('CONNECT without credentials sets only clean session', () => {
    expect(connectPacket('c', {})[9]).toBe(0x02);
  });

  it('PUBLISH QoS 0: topic then payload', () => {
    const p = publishPacket('a/b', '{}');
    expect(bytes(p)).toEqual([0x30, 7, 0, 3, 97, 47, 98, 123, 125]);
  });

  it('DISCONNECT', () => {
    expect(bytes(disconnectPacket())).toEqual([0xe0, 0]);
  });
});
