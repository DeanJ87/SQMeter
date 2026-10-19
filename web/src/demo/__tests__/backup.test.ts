import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { demoDevice } from '../device';
import { backupReply } from '../backup';

// The demo answers backup and restore with the device's own code
// (lib/ConfigBackup in the WebAssembly core; specs/033-config-backup FR-014).

const route = (name: 'backup' | 'preview' | 'apply', body: unknown) => {
  const { reply, restarting } = backupReply(demoDevice.backupCore(), name, JSON.stringify(body));
  return { status: reply.status, body: JSON.parse(reply.body) as Record<string, unknown>, restarting };
};

describe('demo backup and restore', () => {
  beforeAll(async () => {
    await demoDevice.start();
  });
  afterAll(() => demoDevice.stop());

  it('backs up without secrets, and with them encrypted', () => {
    demoDevice.applyConfig(JSON.stringify({ mqtt: { password: 'demo-mqtt-secret' } }));
    const plain = route('backup', {});
    expect(plain.status).toBe(200);
    expect(JSON.stringify(plain.body)).not.toContain('demo-mqtt-secret');
    expect(plain.body.secrets).toBeUndefined();
    const sealed = route('backup', { passphrase: 'demo passphrase' });
    expect(JSON.stringify(sealed.body)).not.toContain('demo-mqtt-secret');
    expect((sealed.body.secrets as { cipher: string }).cipher).toBe('aes-256-gcm');
    expect(route('backup', { passphrase: 'short' }).body.code).toBe('passphrase-short');
  });

  it('restores exactly what it previewed, secrets with the passphrase only', () => {
    const file = JSON.stringify(route('backup', { passphrase: 'demo passphrase' }).body);
    demoDevice.applyConfig(JSON.stringify({ mqtt: { broker: 'changed.lan', password: 'other' } }));
    expect(route('preview', { backup: file, passphrase: 'wrong one' }).body.code).toBe('wrong-passphrase');
    const preview = route('preview', { backup: file, passphrase: 'demo passphrase' });
    expect(preview.status).toBe(200);
    expect((preview.body.secrets as { changed: string[] }).changed).toContain('mqtt.password');
    expect(route('apply', { token: 'not-the-token' }).status).toBe(409);
    const applied = route('apply', { token: preview.body.token });
    expect(applied.status).toBe(200);
    expect(demoDevice.rawConfig().mqtt.broker).not.toBe('changed.lan');
    expect(route('apply', { token: preview.body.token }).status).toBe(409); // used once
  });

  it('refuses a file that is not a backup', () => {
    expect(route('preview', { backup: 'hello' }).body.code).toBe('not-backup');
  });
});
