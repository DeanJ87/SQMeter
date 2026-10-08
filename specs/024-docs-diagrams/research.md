# Research: Diagrams in the Docs (024)

## R1. Renderer: Material's native Mermaid support

- **Decision**: Use MkDocs Material's built-in Mermaid integration: the `pymdownx.superfences` custom fence `mermaid` with `fence_code_format`, as PR #30 set it up.
- **Rationale**: No plugin is needed, and fences stay plain Markdown that GitHub also renders. Material styles diagrams from its CSS variables (`--md-mermaid-*`), which default to the site's accent (cyan), code and background colours. The site palette in `docs/stylesheets/sqmeter.css` therefore reaches every diagram without per-diagram styling (FR-003).
- **Alternatives**:
  - `mkdocs-mermaid2-plugin`: adds a dependency and duplicates what Material does.
  - Pre-rendered SVGs: not reviewable as text (FR-001), and they lose theme colours.

## R2. Self-hosting the renderer (FR-004)

- **Finding**: Material 9.7.7's bundle loads `https://unpkg.com/mermaid@11/dist/mermaid.min.js` only when `typeof mermaid == "undefined"`. A global `mermaid` defined before the bundle runs is used as-is. (Verified in `bundle.d7400e89.min.js`: `function as(){return typeof mermaid=="undefined"||mermaid instanceof Element?_t("https://unpkg.com/mermaid@11/...")...}`.)
- **Decision**:
  - Pin **mermaid 11.17.2**, the latest 11.x. Material is written against the v11 API; v12 is not used.
  - Pin it as a devDependency in `web/package.json`. The web toolchain is already installed in the docs CI job.
  - `npm run docs:vendor` copies `dist/mermaid.min.js` to `docs/assets/javascripts/vendor/mermaid.min.js`. That path is ignored by git (about 2.7 MB of generated output).
  - A theme override (`overrides/main.html`, block `scripts`) adds `<script src="assets/javascripts/vendor/mermaid.min.js">` before Material's bundle, and **only on pages that contain a diagram**. Other pages don't pay the 2.7 MB.
- **Guard**:
  - `tools/docs/diagrams.py --site site` fails the build if any built page contains a diagram but not the vendored script.
  - It also fails if any page references `unpkg.com` or `cdn.jsdelivr`, or if the vendored file is missing from `site/`.
  - Locally, `mkdocs serve` without `docs:vendor` still works but falls back to unpkg. The CI site check catches this before deploy.
- **Pin of Material**: CI installs `mkdocs-material` unpinned, and R2 depends on Material's loading behaviour, so `docs/requirements.txt` pins `mkdocs-material==9.7.7` and CI installs from it.
- **Alternatives**:
  - Commit `mermaid.min.js`: 2.7 MB of churn in git on every upgrade.
  - Load it in `extrahead` on every page: costs 2.7 MB on pages without diagrams.

## R3. Validating syntax and rendering (FR-006)

- **Decision**: `web/scripts/check-diagrams.mjs` does the following:
  1. Extracts every ` ```mermaid ` block from `docs/**/*.md` and `README.md`.
  2. Loads the vendored mermaid in headless Chromium (Playwright, already a web devDependency; CI uses the runner's Chrome, as the screenshot job does).
  3. Runs `mermaid.parse()` and then `mermaid.render()` on each block.
  4. On failure it exits non-zero, naming `file:line` and the diagram ID.
- **Rationale**: Mermaid's parser needs a DOM, and running the real renderer catches render-time errors that parse alone misses. It's the same library and version the site serves.
- **Alternatives**:
  - `@mermaid-js/mermaid-cli`: pulls in Puppeteer and a second Chromium.
  - jsdom: mermaid's layout needs real SVG text measurement.

## R4. Diagram metadata, completeness and freshness (FR-005, FR-007, SC-002, SC-004)

- **Decision**: Each diagram is preceded by one HTML comment, which is invisible on the site and on GitHub:

  ```
  <!-- diagram: DIA-02
  sources: lib/AlpacaLogic/src/SafetyEvaluator.cpp lib/DeviceCore/src/DeviceCore.cpp#safetyInputs
  blocking: true
  fingerprint: 3f1a…
  -->
  ```

  `tools/docs/diagrams.py` (Python standard library only) checks:
  - **Completeness**: the metadata exists; the diagram has `accTitle`/`accDescr`, a caption (`<figcaption>`, or a `*Figure:*` line in the README), and a "Diagram in words" block. A missing piece is an error.
  - **Freshness**: it hashes the sources and compares the result with the recorded fingerprint. A stale diagram is a warning; a stale `blocking: true` diagram is an error (Principle I: safety-verdict code). `--confirm DIA-xx` rewrites the fingerprint, which is the deliberate step.
  - **Single source for DIA-01** (FR-012): every copy of one ID must have an identical body.
- **Symbol-scoped sources**: `path#symbol` hashes only the named function's definition, found by name and brace matching. Without this, any edit to the 1,000-line `DeviceCore.cpp` or `WebServer.cpp` would mark safety diagrams stale and block unrelated pull requests, such as specs 020 and 021, which are in flight. A symbol that can't be found is an error, so renames surface.
- **Fingerprint**: SHA-256 over `path\n` plus content for each source in the listed order, truncated to 16 hex digits. Directories hash every file beneath them, sorted.
- **Where it runs**:
  - A new `.github/workflows/diagrams.yml` (Python only) runs on pull requests touching `src/`, `include/`, `lib/`, `docs/`, `README.md`, `web/src/demo/`, `tools/` or the workflow itself.
  - The docs job runs `--site`.
  - `/speckit-converge` and contributors run it locally (quickstart).
- **Alternatives**:
  - Git-diff based ("did a source change in this PR"): misses changes made in earlier PRs and depends on the base.
  - Whole-file hashes only: too coarse for the large files (see above).

## R5. Accessibility of diagrams (FR-005, spec 022)

- **Decision**: Every diagram has three text layers:
  1. Mermaid `accTitle:` and `accDescr:`, which mermaid turns into the SVG's `<title>`/`<desc>` and `aria-labelledby`/`aria-describedby`.
  2. A visible `<figcaption>`.
  3. A collapsible `??? info "Diagram in words"`, which is a native `<details>` and works without JavaScript, holding the full description as a list.
- No `classDef`/`style` colours inside diagrams. Safe/unsafe states are named in words, not shown by colour (FR-003).
- Wide diagrams scroll inside `.md-typeset .mermaid` (`overflow-x: auto`), and flowcharts with more than about 4 columns use `TB` (FR-002).

## R6. Accuracy findings while reading the code

Where the code and the existing docs or spec disagree, the diagrams follow the **code**:

1. **First setup does restart.** Spec 024's Context says setup "now joins WiFi without the reboot" PR #30 shows. In the code (`WebServer::pollWiFiConnect`, `main.cpp`), the device:
   - joins the network while still running the SQM-Setup hotspot;
   - saves the credentials only after it has connected;
   - lets the setup screen show the new address;
   - **restarts about 15 s later** (`scheduleRestart(15000)`, plus `connectedFromHotspotFor(15000)` in `main.cpp`).

   What changed since PR #30 is that the device joins *before* rebooting and the portal opens by itself (`CaptiveDns`, probe URLs). DIA-07 shows the restart.
2. **OTA rollback is limited to image verification.** The firmware never calls `esp_ota_mark_app_valid_cancel_rollback`, and the Arduino core's bootloader rollback isn't enabled, so a new image that *boots but crashes* is **not** rolled back automatically. What does hold:
   - `Update.end()` verifies the image before switching the boot partition, so a corrupt or partial download never becomes the boot image.
   - The filesystem is written first and the firmware last, so a failure before the switch leaves the device on its old firmware.

   DIA-08 shows exactly this. `docs/user-guide/ota.md`'s "If the new firmware fails to boot, the bootloader stays on the old slot" overstates it, and is corrected next to the diagram.
3. **Alerts while off.** The alert engine always runs, so its state tracks reality. When alerts are switched off ("not imaging"), nothing goes out:
   - The push channels additionally need the master switch (`alerts.enabled`).
   - A **Wake**-level alert still rings paired Bluetooth phones when the master switch is off, but only while alerts are switched on (`alertsArmed`).

   DIA-06 shows both gates.
4. **`docs/hardware/rg15.md` says "Most ESP32 GPIO inputs are 5 V-tolerant"**. That is not true of the ESP32. DIA-13 doesn't repeat it; the claim is reported as a follow-up, outside this feature's scope.
5. **`docs/hardware/overview.md`** omits the anemometer and BLE. DIA-13 includes them, with the pin defaults from `ConfigModel::createDefault`:
   - I²C on 21/22
   - GPS: RX 17, TX 16
   - RG-15: RX 18, TX 19
   - anemometer on 27, vane on 35 (ADC1)

## R7. Coding-standard rule (FR-009)

- Spec 017's `docs/development/coding-standards.md` isn't on main yet (PR #89), so the rule goes into `CONTRIBUTING.md` and `docs/development/contributing.md` now: "a change to behaviour shown in a diagram updates the diagram and re-confirms it".
- The PR notes that spec 017 should adopt it as a numbered rule (DOC-01) when it lands.

## R8. PR #30 reconciliation (FR-011)

- Its `mkdocs.yml` fence configuration is reused verbatim.
- Its four diagrams are redrawn against the current code: DIA-01 (index), DIA-15 (integrations), DIA-07 (first setup), DIA-08 (OTA).
- The implementation PR credits #30 and states it supersedes it. #30 is closed only when this merges.
