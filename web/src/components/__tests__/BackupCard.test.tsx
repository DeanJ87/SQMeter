import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import BackupCard from '../settings/BackupCard';
import { Toaster } from '../toast';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';

// Settings → Device → Backup & restore (specs/033-config-backup contracts/ui.md).

const fileInput = (container: Element) => container.querySelector('input[type="file"]') as HTMLInputElement;
const choose = (container: Element, text: string, name = 'backup.json') =>
  fireEvent.change(fileInput(container), { target: { files: [new File([text], name, { type: 'application/json' })] } });
const encrypted = JSON.stringify({ sqmeterBackup: 1, deviceName: 'backyard-sqm', settings: mockConfig, secrets: { kdf: 'pbkdf2-sha256' } });
const plain = JSON.stringify({ sqmeterBackup: 1, deviceName: 'backyard-sqm', firmware: '0.3.4-beta.1', settings: mockConfig });

const renderCard = () =>
  render(
    <>
      <BackupCard deviceName="backyard-sqm" status={null} />
      <Toaster />
    </>,
  );

describe('Backup & restore card', () => {
  beforeEach(() => {
    vi.stubGlobal('URL', Object.assign(URL, { createObjectURL: vi.fn(() => 'blob:backup'), revokeObjectURL: vi.fn() }));
  });
  afterEach(() => vi.unstubAllGlobals());

  it('backs up without a passphrase and says the file has no secrets', async () => {
    let body: unknown;
    server.use(
      http.post('/api/backup', async ({ request }) => {
        body = await request.json();
        return HttpResponse.json({ sqmeterBackup: 1, settings: mockConfig });
      }),
    );
    renderCard();
    fireEvent.click(screen.getByRole('button', { name: 'Back up settings' }));
    expect(await screen.findByText('Backup saved without passwords and keys.')).toBeInTheDocument();
    expect(body).toEqual({});
    expect(URL.createObjectURL).toHaveBeenCalled();
  });

  it('asks for the passphrase twice and checks it', async () => {
    let sent: unknown;
    server.use(
      http.post('/api/backup', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({ sqmeterBackup: 1, settings: mockConfig, secrets: {} });
      }),
    );
    renderCard();
    fireEvent.input(screen.getByLabelText('Passphrase', { selector: 'input' }), { target: { value: 'short' } });
    fireEvent.click(screen.getByRole('button', { name: 'Back up settings' }));
    expect(await screen.findByText('At least 8 characters')).toBeInTheDocument();
    fireEvent.input(screen.getByLabelText('Passphrase', { selector: 'input' }), { target: { value: 'a long passphrase' } });
    fireEvent.input(screen.getByLabelText('Repeat passphrase', { selector: 'input' }), { target: { value: 'a long passphrase!' } });
    fireEvent.click(screen.getByRole('button', { name: 'Back up settings' }));
    expect(await screen.findByText("Passphrases don't match")).toBeInTheDocument();
    expect(sent).toBeUndefined();
    fireEvent.input(screen.getByLabelText('Repeat passphrase', { selector: 'input' }), { target: { value: 'a long passphrase' } });
    fireEvent.click(screen.getByRole('button', { name: 'Back up settings' }));
    expect(await screen.findByText('Backup saved with encrypted passwords and keys.')).toBeInTheDocument();
    expect(sent).toEqual({ passphrase: 'a long passphrase' });
  });

  it('previews a backup and restores it after confirming', async () => {
    let applied = '';
    server.use(
      http.post('/api/backup/apply', async ({ request }) => {
        applied = ((await request.json()) as { token: string }).token;
        return HttpResponse.json({ success: true, restarting: false });
      }),
    );
    const { container } = renderCard();
    choose(container, plain);
    expect(await screen.findByText('mqtt.broker')).toBeInTheDocument();
    expect(screen.getByText(`${mockConfig.mqtt.broker || 'empty'} → broker.lan`)).toBeInTheDocument();
    expect(applied).toBe('');
    fireEvent.click(screen.getByRole('button', { name: 'Restore' }));
    expect(await screen.findByText('Settings restored')).toBeInTheDocument();
    expect(applied).toBe('00112233445566778899aabbccddeeff');
  });

  it('changes nothing when cancelled', async () => {
    let applied = false;
    server.use(http.post('/api/backup/apply', () => ((applied = true), HttpResponse.json({ success: true }))));
    const { container } = renderCard();
    choose(container, plain);
    fireEvent.click(await screen.findByRole('button', { name: 'Cancel' }));
    await waitFor(() => expect(screen.queryByText('mqtt.broker')).not.toBeInTheDocument());
    expect(applied).toBe(false);
  });

  it('asks for the file passphrase and says when it is wrong', async () => {
    let sent: unknown;
    server.use(
      http.post('/api/backup/preview', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json(
          { error: "Wrong passphrase, or the file's secrets were changed", code: 'wrong-passphrase' },
          { status: 400 },
        );
      }),
    );
    const { container } = renderCard();
    choose(container, encrypted);
    fireEvent.input(await screen.findByLabelText('Passphrase for this file', { selector: 'input' }), {
      target: { value: 'not it at all' },
    });
    fireEvent.click(screen.getByRole('button', { name: 'Preview' }));
    expect(await screen.findByText("Wrong passphrase, or the file's secrets were changed")).toBeInTheDocument();
    expect(sent).toEqual({ backup: encrypted, passphrase: 'not it at all' });
  });

  it('shows why a file is refused', async () => {
    server.use(
      http.post('/api/backup/preview', () =>
        HttpResponse.json({ error: "This isn't a SQMeter backup file", code: 'not-backup' }, { status: 400 }),
      ),
    );
    const { container } = renderCard();
    choose(container, 'not json', 'notes.txt');
    expect(await screen.findByText("This isn't a SQMeter backup file")).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Restore' })).not.toBeInTheDocument();
  });

  it('refuses a file over 64 KB before reading it', async () => {
    const { container } = renderCard();
    choose(container, 'x'.repeat(65 * 1024));
    expect(await screen.findByText("The file is over 64 KB, so it isn't a SQMeter backup.")).toBeInTheDocument();
  });

  it('says when nothing will change', async () => {
    server.use(
      http.post('/api/backup/preview', () =>
        HttpResponse.json({
          token: 'ab',
          backup: { format: 1 },
          settings: mockConfig,
          secrets: { state: 'none', changed: [], unchanged: [] },
          notApplicable: [],
          restart: [],
        }),
      ),
    );
    const { container } = renderCard();
    choose(container, plain);
    expect(await screen.findByText('Nothing will change.')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Restore' })).toBeDisabled();
  });
});
