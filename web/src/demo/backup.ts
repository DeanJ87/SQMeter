import type { Reply } from './device';

// Settings backup and restore in the demo (specs/033-config-backup FR-014):
// the device's own code (lib/ConfigBackup in the WebAssembly core), with the
// salt, nonce and token from the browser's random numbers.

export interface BackupCore {
  backupRoute(route: string, body: string, randomHex: string): string;
}

export type BackupRoute = 'backup' | 'preview' | 'apply';

const RANDOM_BYTES: Record<BackupRoute, number> = { backup: 28, preview: 16, apply: 0 };

const randomHex = (bytes: number) =>
  Array.from(crypto.getRandomValues(new Uint8Array(bytes)), (b) => b.toString(16).padStart(2, '0')).join('');

/** One backup route's reply, and whether the device restarts after it. */
export const backupReply = (core: BackupCore, route: BackupRoute, body: string): { reply: Reply; restarting: boolean } => {
  const answer = JSON.parse(core.backupRoute(route, body, randomHex(RANDOM_BYTES[route]))) as {
    status: number;
    body: { restarting?: boolean };
  };
  return { reply: { status: answer.status, body: JSON.stringify(answer.body) }, restarting: answer.body.restarting === true };
};
