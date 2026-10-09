// A minimal MQTT 3.1.1 publisher over a secure WebSocket, for the demo's
// opt-in real notifications (specs/018 research R6): connect, publish one
// QoS 0 message, disconnect. Brokers need MQTT over WebSockets with TLS.

export interface MqttTarget {
  url: string; // wss://broker.example:8884/mqtt
  username?: string;
  password?: string;
}

const PROTOCOL_LEVEL = 4; // MQTT 3.1.1
const KEEPALIVE_S = 30;
const TIMEOUT_MS = 8000;
// Closing straight after send can drop the PUBLISH on its way out: wait for
// the browser's buffer to drain, plus a moment, before hanging up.
const FLUSH_POLL_MS = 50;
const FLUSH_GRACE_MS = 300;
const FLAG_CLEAN_SESSION = 0x02;
const FLAG_PASSWORD = 0x40;
const FLAG_USERNAME = 0x80;
const PACKET_CONNECT = 0x10;
const PACKET_CONNACK = 0x20;
const PACKET_PUBLISH = 0x30;
const PACKET_DISCONNECT = 0xe0;

const CONNACK_REASONS: Record<number, string> = {
  1: 'the broker refused the MQTT version',
  2: 'the broker refused the client ID',
  3: 'the broker is unavailable',
  4: 'wrong username or password',
  5: 'not authorised',
};

const utf8 = (text: string) => new TextEncoder().encode(text);

const lengthPrefixed = (bytes: Uint8Array) => {
  const out = new Uint8Array(bytes.length + 2);
  out[0] = bytes.length >> 8;
  out[1] = bytes.length & 0xff;
  out.set(bytes, 2);
  return out;
};

const join = (parts: Uint8Array[]) => {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const part of parts) {
    out.set(part, at);
    at += part.length;
  }
  return out;
};

/** MQTT "remaining length": 7 bits per byte, high bit = more follows. */
export const remainingLength = (length: number) => {
  const bytes: number[] = [];
  let value = length;
  do {
    let byte = value % 128;
    value = Math.floor(value / 128);
    if (value > 0) byte |= 0x80;
    bytes.push(byte);
  } while (value > 0);
  return Uint8Array.from(bytes);
};

const packet = (type: number, body: Uint8Array) => join([Uint8Array.of(type), remainingLength(body.length), body]);

export const connectPacket = (clientId: string, target: Pick<MqttTarget, 'username' | 'password'>) => {
  let flags = FLAG_CLEAN_SESSION;
  const payload = [lengthPrefixed(utf8(clientId))];
  if (target.username) {
    flags |= FLAG_USERNAME;
    payload.push(lengthPrefixed(utf8(target.username)));
    if (target.password) {
      flags |= FLAG_PASSWORD;
      payload.push(lengthPrefixed(utf8(target.password)));
    }
  }
  const header = join([lengthPrefixed(utf8('MQTT')), Uint8Array.of(PROTOCOL_LEVEL, flags, KEEPALIVE_S >> 8, KEEPALIVE_S & 0xff)]);
  return packet(PACKET_CONNECT, join([header, ...payload]));
};

export const publishPacket = (topic: string, payload: string) => packet(PACKET_PUBLISH, join([lengthPrefixed(utf8(topic)), utf8(payload)]));

export const disconnectPacket = () => Uint8Array.of(PACKET_DISCONNECT, 0);

const flushed = (socket: WebSocket) =>
  new Promise<void>((resolve) => {
    const poll = () => (socket.bufferedAmount === 0 ? setTimeout(resolve, FLUSH_GRACE_MS) : setTimeout(poll, FLUSH_POLL_MS));
    poll();
  });

/** Resolves with "Published", rejects with a plain-words reason. */
export function mqttPublish(target: MqttTarget, topic: string, payload: string): Promise<string> {
  return new Promise((resolve, reject) => {
    let settled = false;
    let published = false;
    let socket: WebSocket;
    const finish = (error: string | null) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      if (error) reject(new Error(error));
      else resolve('Published');
      try {
        socket.close();
      } catch {
        // already closed
      }
    };
    const timer = setTimeout(() => finish("The broker didn't answer within 8 s"), TIMEOUT_MS);
    try {
      socket = new WebSocket(target.url, ['mqtt']);
    } catch {
      finish('That broker address isn’t a valid wss:// URL');
      return;
    }
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => socket.send(connectPacket(`sqmeter-demo-${Math.random().toString(16).slice(2, 8)}`, target));
    socket.onmessage = (event) => {
      const bytes = new Uint8Array(event.data as ArrayBuffer);
      if ((bytes[0] & 0xf0) !== PACKET_CONNACK) return;
      const code = bytes[3];
      if (code !== 0) {
        finish(`The broker refused the connection: ${CONNACK_REASONS[code] ?? `code ${code}`}`);
        return;
      }
      socket.send(publishPacket(topic, payload));
      socket.send(disconnectPacket());
      published = true;
      void flushed(socket).then(() => finish(null));
    };
    socket.onerror = () => finish("This broker didn't accept a browser connection - it needs MQTT over secure WebSockets");
    // A broker may hang up as soon as it sees DISCONNECT: that's success.
    socket.onclose = () => finish(published ? null : 'The broker closed the connection before accepting it');
  });
}
