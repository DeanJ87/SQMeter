// Makes webcrypto-backup.json: a backup whose secrets were encrypted by Node's
// WebCrypto, an implementation independent of lib/ConfigBackup, so the native
// tests prove the device reads files made elsewhere (specs/033-config-backup,
// research R2). Run: node test/fixtures/backup/make-webcrypto-fixture.mjs
import { webcrypto as crypto } from 'node:crypto';
import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const PASSPHRASE = 'correct horse battery';
const ITERATIONS = 20000;
const SECRETS = {
  'wifi.password': 'observatory-wifi',
  'mqtt.password': 'broker-secret',
  'ota.password': '',
  'auth.password': 'admin-pass',
  'ble.passkey': '123456',
  'alerts.pushover.userKey': '',
  'alerts.pushover.appToken': '',
  'alerts.ntfy.token': 'tk_ntfy',
  'alerts.webhook.authHeader': 'Bearer abc',
};

const salt = Uint8Array.from({ length: 16 }, (_, i) => i * 7 + 3);
const nonce = Uint8Array.from({ length: 12 }, (_, i) => 200 - i * 5);
const encoder = new TextEncoder();
const base = await crypto.subtle.importKey('raw', encoder.encode(PASSPHRASE), 'PBKDF2', false, ['deriveKey']);
const key = await crypto.subtle.deriveKey(
  { name: 'PBKDF2', hash: 'SHA-256', salt, iterations: ITERATIONS },
  base,
  { name: 'AES-GCM', length: 256 },
  false,
  ['encrypt'],
);
const sealed = await crypto.subtle.encrypt(
  { name: 'AES-GCM', iv: nonce, additionalData: encoder.encode('sqmeter-backup/1'), tagLength: 128 },
  key,
  encoder.encode(JSON.stringify(SECRETS)),
);
const b64 = (bytes) => Buffer.from(bytes).toString('base64');

const file = {
  sqmeterBackup: 1,
  firmware: '0.3.4-beta.1',
  board: 'esp32',
  build: 'ble',
  deviceName: 'webcrypto-fixture',
  createdAt: '2026-10-18T21:04:05Z',
  settings: {
    deviceName: 'webcrypto-fixture',
    wifi: { ssid: 'Observatory', password: '********' },
    mqtt: { enabled: true, broker: 'broker.lan', port: 1883, username: 'sqm', password: '********', topic: 'sqmeter' },
    auth: { enabled: true, username: 'admin', password: '********' },
  },
  secrets: { kdf: 'pbkdf2-sha256', iterations: ITERATIONS, salt: b64(salt), cipher: 'aes-256-gcm', nonce: b64(nonce), data: b64(new Uint8Array(sealed)) },
};
writeFileSync(join(dirname(fileURLToPath(import.meta.url)), 'webcrypto-backup.json'), JSON.stringify(file, null, 2) + '\n');
