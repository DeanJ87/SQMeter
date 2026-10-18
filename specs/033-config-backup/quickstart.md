# Quickstart: validating settings backup and restore

## Prerequisites

- PlatformIO 6.2+ (native tests, both firmware builds); Node 20+ in `web/`; Emscripten (see
  `tools/demo-core/VERSION`) only to rebuild the demo core.
- For device checks: the spare test device, never the main outdoor one.

## 1. Logic (native)

```bash
pio test -e native -f test_config_backup
```

Covers: crypto against published vectors and a WebCrypto-made fixture; backup without a passphrase holds
no secret (SC-002); with one, the secrets round-trip; wrong passphrase, changed ciphertext, damaged files,
not-a-backup, newer format and a format-0 export (SC-003); unchanged backup → no changes; cross-board pins
listed as not applicable (SC-005); kept/ignored settings; restart and warning codes (shared fixture).

## 2. Web UI and demo

```bash
cd web && npx vitest run src/components/__tests__/BackupCard.test.tsx src/demo/__tests__/backup.test.ts
npm run build:demo && npm run preview:demo
```

In the demo: Settings → Device → Backup & restore.

1. Back up without a passphrase → file downloads; search it for each password: none present.
2. Type a passphrase twice, back up → file has a `secrets` block.
3. Change the MQTT broker and the WiFi password, save. Restore the second file with the passphrase →
   preview lists `mqtt.broker` and "WiFi password: Will change", restart WiFi; Restore → settings match.
4. Restore with a wrong passphrase → "Wrong passphrase…", nothing changes.
5. Restore without the passphrase → "Kept as on this device"; the rest restores.
6. Restore the same file again → "Nothing will change."
7. Upload a PNG, and the backup with a character deleted → refused with the reason.

## 3. Contract and docs

```bash
python3 tools/api/openapi.py --check
python3 tools/api/routes.py
mkdocs build --strict
```

## 4. Device (spare device only)

1. Flash both builds to the spare device in turn; back up with a passphrase.
2. Note `freeHeap` in `/api/status` before and during a preview; read the serial log line
   `Backup key derived in N ms` (expected 1-2 s).
3. Factory-reset the spare device (or flash a clean config), join WiFi, restore the file with the
   passphrase → it rejoins WiFi, MQTT reconnects, a test alert arrives (SC-001).
