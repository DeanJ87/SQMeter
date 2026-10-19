# Contract: UI (Settings → Device → Backup & restore)

One `SettingsCard` titled **Backup & restore**, after Security. DS rules: DS-01/DS-03 (built from
`SettingsCard`, `Field`, `Group`, `ReadingRow`), DS-04/DS-23 (the explanations in "?"), DS-05 (paths and
values one per row, monospace, truncated with `title`), DS-06 (progress and outcome in this card), DS-07
(the "restored" confirmation is a toast), DS-20..DS-22 (terse copy), V (no banners).

| State | What shows |
|---|---|
| Idle | Card "?" (what the file holds). `Passphrase` field ("?": optional, 8+ characters, needed to restore passwords and keys, can't be recovered). `Repeat passphrase` once a passphrase is typed. Buttons **Back up settings**, **Restore settings**. |
| Backing up | Back up button busy ("Working…"). |
| Backed up | Note ok: "Backup saved with encrypted passwords and keys." or "Backup saved without passwords and keys." |
| Passphrase errors | Field errors: "At least 8 characters", "Passphrases don't match". |
| File chosen | Group **Restore** with rows File, Device, Made, Firmware, Build. If the file has encrypted secrets: `Passphrase for this file` field (optional). Buttons **Preview**, **Cancel**. A file without secrets previews at once. |
| Previewing | Preview button busy. |
| Preview | Group per section (section label) with one `ReadingRow` per change: path → "old → new". Group **Passwords and keys**: a row per secret that will change and a Note counting the unchanged ones ("All unchanged." when none change), or one Note "Kept as on this device" / "Not in the backup". Groups **Not in the backup** (kept), **Unknown to this firmware** (ignored) and **Can't apply here** (not applicable, with reason), each a list of paths. Warning Notes (restart, WiFi network, password, address, imaging app, newer firmware). Buttons **Restore**, **Cancel**. |
| No changes | Note: "Nothing will change." Restore disabled. |
| Refused | Note bad with the reason: wrong passphrase, damaged, not a backup, newer format, too large, or the save's validation message. Button **Cancel**. |
| Restoring | Restore button busy; with a restart, Note "Restarting…" then the page reloads when the device answers. |
| Restored | Toast "Settings restored"; the card returns to idle and the settings reload. |

Phone (390 px): the same order in one column; buttons wrap (`btn-row`).
