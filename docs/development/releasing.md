# Releasing

Releases are made from tags. Until the first stable release, every tag is a beta: `vX.Y.Z-beta.N`. GitHub publishes it as a pre-release, which devices on the beta track are offered.

## 1. Changelog pull request

Move the entries under `## [Unreleased]` in `CHANGELOG.md` into a new section:

```markdown
## [0.3.3-beta.1] — 2026-10-20

### Fixed

- **Short title**: what changed and why it matters.
```

- The heading needs the version in brackets and an ISO date.
- Start the most important bullets with a **bold title**. The release banner shows the first three bold titles, in the order they're written (anything under *Planned* or *Internal* is skipped).

The pull request runs the **Release notes** check, which renders the notes and banner for the newest version in the changelog. Download the `release-notes-preview` artifact from the check to see them.

To preview locally:

```bash
cd web && npm ci && npx playwright install chromium && cd ..
node tools/release/notes.mjs --tag v0.3.3-beta.1 --dry-run   # writes release-notes/
```

## 2. Tag

Once the changelog is merged, tag `main` and push the tag:

```bash
git fetch origin
git tag -a v0.3.3-beta.1 -m "SQMeter v0.3.3-beta.1" origin/main
git push origin v0.3.3-beta.1
```

## 3. The release job

The tag starts the **Build Firmware & LittleFS** workflow. Its release job:

1. builds both firmware variants and the web UI with the version stamped in, and the USB flash packages;
2. builds the language files and the OpenAPI description for this version;
3. writes the release notes and banner from the changelog section (`tools/release/notes.mjs`). If `CHANGELOG.md` has no section for the tag, the job stops here and nothing is published;
4. publishes the release with the banner at the top, the changelog section, how to upgrade, and GitHub's list of merged pull requests.

The banner is attached as `sqmeter-release-<tag>.png` (and `@2x`). The docs address in the upgrade note comes from `site_url` in `mkdocs.yml`.

## 4. Check it

Update a test device from the Updates page (beta track) and confirm the version, settings and WiFi are unchanged.
