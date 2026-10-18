# Mockups: settings backup and restore

Every state in [contracts/ui.md](../contracts/ui.md), drawn with the app's own markup and CSS (the demo
build, rewritten in the browser with Playwright; nothing here is shipped). Each state has a
`-desktop.png` (1280 px) and a `-phone.png` (390 px). The card sits on Settings → Device after
Security.

## Back up

| File | State | DS rules |
|---|---|---|
| `b1-idle` | Card "?" (what the file holds); `Passphrase` with "?" (optional, 8+ characters, needed for passwords and keys, can't be recovered); **Back up settings**, **Restore settings** | DS-01, DS-04, DS-20, DS-23 |
| `b2-backed-up` | Passphrase typed twice; Note ok "Backup saved with encrypted passwords and keys." | DS-06, DS-22 |
| `b3-passphrase-mismatch` | `Repeat passphrase` field error "Passphrases don't match" | DS-22 |
| `b4-backed-up-no-passphrase` | Note ok "Backup saved without passwords and keys." | DS-22 |

## Restore

| File | State | DS rules |
|---|---|---|
| `r1-file-chosen` | Group **Restore**: File, Device, Made, Firmware, Board rows; the file has encrypted secrets, so `Passphrase for this file` ("?": without it this device keeps its own); **Preview**, **Cancel** | DS-03, DS-05 |
| `r2-preview` | One group per section with a row per change (path → "old → new"); **Passwords and keys** ("Will change" / "Unchanged", the rest counted); **Can't apply here** (reason); **Not in the backup** (kept); **Unknown to this firmware** (ignored); warning Notes (restart, other WiFi network with the way back, new local name); **Restore**, **Cancel** | DS-03, DS-05, DS-22, DS-24 |
| `r3-preview-secrets-kept` | No passphrase given: one Note "Kept as on this device: no passphrase given." | DS-22 |
| `r4-no-changes` | Note "Nothing will change."; Restore disabled | DS-22 |
| `r5-wrong-passphrase` | Field error "Wrong passphrase, or the file's secrets were changed"; nothing changed | DS-22 |
| `r6-damaged` | Note bad "This isn't a SQMeter backup file" (also: damaged, newer format, too large, the save's own reason) | DS-07, DS-22 |
| `r7-restarting` | Note "Settings restored. Restarting…"; the page reloads when the device answers | DS-06 |
| `r8-restored` | Toast "Settings restored"; the card back to idle | DS-07 |

The phone layout keeps the same order in one column; long values (file name, paths) wrap or truncate
with the full value in `title` (DS-05).

## Owner review

Pending. The card was built from these mockups (owner's rule: mock before building). Where the build
differs: the file rows show **Build** (Standard / Bluetooth) instead of a combined "Board" value (DS-24:
one fact per row); the unchanged secrets are counted in one Note, which reads "All unchanged." when no
secret changes; dates follow the browser's language.
