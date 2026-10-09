# Reconcile pass, October 2026

Before v0.2.0-beta.4: is `main` correct, consistent and documented after specs 015–026 landed
in quick succession? Run on `main` at 9318f21 (after #119).

## Fixed

| Area | Finding | Fixed in |
|---|---|---|
| 023 alerts | Custom alert wording over ~27 multi-byte characters was rejected by the device (bytes) while the UI allowed 80 (characters) | #119 |
| 023 alerts | Test-alert texts, event labels, delivery details, delivery results, alert ages and the level/sound labels were English in every language; device-reported dependency reasons too | #119 |
| 026 alerts | "Reset" in the wording editor looked like a primary action and acted at once | #119 (link + in-place confirm) |
| 010/025 dashboard | A new location only reached Sun & Moon after a reload | #119 (`configRevision`) |
| 008 alerts | New alerts reached the bell only on a 20 s poll | #119 (`alerts.recentRevision`, pushed) |
| 017 | The bell and the page opened two status sockets on a device with few connections | #119 (one socket per URL) |
| Docs | `alerts.md` named **Settings → Safety → Alpaca** (card is "ASCOM Alpaca"), described the bell's raw `sent`/`failed`/`skipped` words, and `troubleshooting.md` said alerts are "switched off" (now "paused", spec 021) | this PR |
| DS-27 | The dashboard said "Wi-Fi" while Settings and the docs say "WiFi" | this PR (glossary entry enforces it) |
| Specs | Status lines out of date: 017 (burn-down progress), 023, 025 ("Draft"), 026 (no PRs); 010 FR-001 still said failed sensors' cards are hidden (025 FR-013 keeps them) | this PR |
| Drift guard | Nothing checked that docs' UI paths still exist | this PR: `tools/docs/ui_paths.py` (DS-28, DS-PATH in the quality gate) |

## Checked, no change needed

- Every spec 001–026 has a status line and no unticked task, except those below.
- 18 diagrams re-confirmed against their sources; none needed redrawing.
- Copy (DS-20..25), labels (DS-27), docs UI paths (DS-28), i18n completeness (13 languages), device-message catalogue, route registry, dashboard inventory, UI size budget: all pass.

## Left open (with reasons)

| Item | Why |
|---|---|
| 017 T018: split `tools/demo-core/bridge.cpp` (768 lines, the one baselined finding) | Needs the emulated device's class moved to a header and the WebAssembly core rebuilt; no user-facing effect. Do it with the next demo-core change. |
| 015 T010: mDNS AAAA answers | Needs an IPv6 client on the device's subnet. |
| 022 manual checks (keyboard, VoiceOver/NVDA, zoom, real phone) | Need a person. |
| 020/021 MQTT and Home Assistant checks | Need the broker login. |
| 018/008 Pushover delivery | Needs the user's Pushover keys. |
| Device smoke on the spare (contract check, ConformU, every page, language, OTA) | Deferred until the Arduino-ESP32 3.x decision (spec 027), so it runs once on the firmware that ships. |
