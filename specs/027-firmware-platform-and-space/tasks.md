# Tasks: Firmware platform and space

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md)

## Phase 1: Budget (US1, P1)

- [x] T001 [US1] `tools/firmware/flash_budget.py` and `size_map.py`: both builds' size against the slot, warn ≥ 90%, fail ≥ 95%, largest contributors; tests in `tools/firmware/test_firmware_tools.py` (FR-001, FR-002)
- [x] T002 [US1] SIZE-04 in docs/development/coding-standards.md and the constitution (1.3.0) (FR-003)
- [x] T003 [US1] CI: budget step in .github/workflows/build.yml on PlatformIO 6.2 with clean builds

## Phase 2: Platform (US3, P2)

- [x] T004 [US3] pioarduino 55.03.312-1 for both builds and native tests; ESP32Async libraries; NimBLE 2.5.1; `-Werror` on src/ (FR-007)
- [x] T005 [US3] API changes fixed without stubs; counters shared between tasks made atomic (FR-008)
- [x] T006 [US3] IPv6 listener and discovery peer check on the 3.x network stack; IPv6 for TLS, HTTPClient and NTP; spec 015 and docs/user-guide/ipv6.md updated (FR-009)
- [x] T007 [US3] `custom_sdkconfig` trims with scripts/check_sdkconfig.py and scripts/clean_sdkconfig.py (FR-004, FR-010, FR-011)

## Phase 3: Trims (US2, P1)

- [x] T008 [US2] `CORE_DEBUG_LEVEL=1`, silent assertions, unused framework components removed (FR-004)
- [x] T009 [US2] Root certificates embedded once (src/RootCertificates.cpp) (FR-005)

## Phase 4: Layout and upgrade (US4, US5)

- [x] T010 [US5] Layout l2 in partitions.csv; SIZE-02 448 KB; LittleFS 2.0 (FR-015)
- [x] T011 [US4] Image marker (src/FirmwareMarker.cpp) and lib/FirmwareImage checks on upload and download; refusal messages translated (FR-020)
- [x] T012 [US4] Rollback: mark valid at the end of setup; `SQM_TEST_BOOT_CRASH` test build (FR-021)
- [x] T013 [US4] `firmware.layout` in /api/status and the Updates page note (FR-017)
- [x] T014 [US4] l2 release file names, USB packages (tools/firmware/usb_package.py) and CI release steps (FR-018, FR-019)
- [x] T015 [US4] docs/getting-started/usb-flash.md with a self-hosted ESP Web Tools flasher and esptool commands; flashing, OTA, BLE, REST and troubleshooting pages; rollback documented (FR-016, FR-017, FR-019)

## Phase 5: Verification

- [x] T016 All gates: native tests, both builds, budget, quality, web, Playwright, docs, diagrams, demo core
- [ ] T017 Spare device: USB flash keeping NVS, pages, contract check, ConformU, update check, NTP, language, OTA, rollback, refusals, heap; record in device-results.md (FR-006, SC-002..SC-005)
