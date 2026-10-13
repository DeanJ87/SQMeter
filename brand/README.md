# SQMeter brand kit

The SQMeter mark is a figure on a dark horizon pointing a cyan beam at the sky: a person measuring the night. Use the files here; don't redraw them.

| Folder | What's in it |
|---|---|
| `src/logo/` | The mark in every variant, the wordmark, and the lockups (source SVG) |
| `src/favicon/`, `src/app-icon/` | Favicons (16, 32, 48 px) and the 512 px app icon |
| `src/docs/` | The docs site's logo and favicon |
| `src/social/` | GitHub social card, README banner, square and story posts, forum header, release banner template |
| `src/sticker/` | Round sticker (60 mm) and enclosure label (70 × 24 mm), colour and one-colour |
| `png/` | Rendered PNGs the repo uses (README banner, social card, apple-touch icon) |
| `sqmeter-tokens.css` | Brand colours, fonts and logo colours |
| `source-README.md` | The designer's original notes |

## Which mark, where

| Size or use | File |
|---|---|
| 40 px and larger | `sqmeter-mark.svg` (detailed: gradient sky, beam glow) |
| 17 to 39 px (UI headers, the docs header) | `sqmeter-mark-flat.svg` |
| 16 px | `favicon/favicon-16.svg` (simplified for 16 px) |
| Red night-vision screens | `sqmeter-mark-night.svg`, or `sqmeter-mark-night-mono.svg` for one colour |
| One colour on screen | `sqmeter-mark-mono.svg` (uses `currentColor`), `-mono-light`, `-mono-dark` |
| On cyan | `sqmeter-mark-inverted.svg` |
| Print on light paper | `sqmeter-mark-print.svg` |
| Silkscreen (PCB) | `sqmeter-mark-silkscreen.svg`, at least 6 mm |
| Emboss, deboss, engraving | `sqmeter-mark-emboss.svg`, at least 20 mm |
| Name next to the mark | `sqmeter-lockup-horizontal.svg` (`-light`, `-night`, `-mono`), at least 120 px wide; `sqmeter-lockup-stacked.svg` |

`sqmeter-mark-light.svg` is not the SQMeter mark: it's a different (reticle and arc) design. Don't use it.

## Rules

- **Clear space:** keep a quarter of the mark's height clear on every side.
- **Lockups:** the gap between mark and name is a third of the mark's height.
- **Grid:** 24 units; the tile is inset 2.25 units with a 2.75-unit corner radius. Don't change the corners, add outlines or put the mark on a second tile.
- **Colour:** dark only. On light backgrounds use the print, mono or light-lockup variants, never the full-colour mark on white.
- **Don't** stretch, rotate, recolour the beam, add effects (shadows, glows, gradients) or set the name in another font.
- **Name:** always "SQMeter": capital S, Q and M, one word.

## Colours and type

The brand colours are the app's colours (`web/src/index.css`, `docs/stylesheets/sqmeter.css`): background `#080d14`, panel `#0d141d`, line `#1b2a38`, text `#e7edf3`, muted `#8b9baa`, cyan accent `#55c7f2`, and green, amber and red for safe, warning and unsafe. The values in `sqmeter-tokens.css` match the app exactly; only some names differ (`--sqm-accent` is `--cyan`, `--sqm-ok`/`-warn`/`-bad` are green/amber/red, `--sqm-line-2` is `--line-soft`). The app adds `--control-edge` (`#55687a`) so form controls meet the 3:1 contrast rule.

Type is Inter for text and JetBrains Mono for readings and code. The app and docs ship their own copies ("Inter SQM", "JetBrains Mono SQM").

## PNG and print files

The social, banner and sticker SVGs contain live text in Inter and JetBrains Mono, which GitHub, chat apps and print shops don't have. Render them with the project's own fonts:

```bash
cd web && npm ci && npx playwright install chromium && cd ..
node tools/brand/render.mjs                    # all of them into brand/render/ (git-ignored)
node tools/brand/render.mjs --only social      # social | icons | sticker
node tools/brand/render.mjs --release v0.3.2-beta.1 --date "11 October 2026" \
  --change "First change" --change "Second change" --change "Third change"
```

Social files come out at 1x and 2x. Stickers and labels come out as print PDFs at their real size (60 mm, 70 × 24 mm) and as 600 dpi PNGs. Print-ready copies also live in the hardware repository.

## Where the brand is used

| Place | Files |
|---|---|
| Device web UI header | `web/src/components/BrandMark.tsx` (the flat mark, inlined) |
| Device and demo favicon | `web/public/favicon.svg`, `web/public/favicon-32.png` |
| Demo site link preview and home-screen icon | `png/sqmeter-social-1280x640.png`, `png/apple-touch-icon-180.png` (copied in by `web/vite.demo.config.ts`) |
| Docs logo, favicon, link preview | `docs/assets/logo.svg`, `docs/assets/favicon.svg`, `docs/assets/social-card.png` |
| README banner | `png/sqmeter-readme-banner@2x.png` |
| GitHub social preview | `.github/social-preview.png`, uploaded by hand in the repository's Settings |
