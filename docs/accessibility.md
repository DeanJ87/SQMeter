# Accessibility

SQMeter aims to be usable by everyone, including at night, on a phone, with a screen reader or with a keyboard only.

## Target

**WCAG 2.2 Level AA**, for:

- the device's web interface (served by the SQMeter and in the [live demo](https://demo.sqmeter.dev/));
- this documentation site.

Reduced motion (WCAG 2.3.3, Level AAA) is also respected, because the interface is used in the dark.

## Status

| Area | Status | Last checked |
|---|---|---|
| Device web interface (all pages, including WiFi setup) | **Partially conformant**: automated checks pass on every page; the manual screen-reader audit is in progress | 2026-10-08 |
| Live demo | Same as the device interface. The Demo panel is included in the checks | 2026-10-08 |
| Documentation (this site) | **Partially conformant**: automated checks pass on every page; the third-party theme has the known exceptions below | 2026-10-08 |

"Partially conformant" means some parts don't yet fully meet the standard; they are listed below.

### What has been done

- Every page is checked automatically on each change, at desktop size and at 320 px wide, with the dialogs open, with a validation error and with an unsafe verdict.
- All text and controls meet the contrast minimums of the dark theme.
- Every control is labelled, and validation errors are tied to their field.
- A visible focus ring, a "Skip to main content" link, and Settings tabs that work with the arrow keys.
- The "?" help tips open on tap and focus, are read by screen readers, and close with Escape.
- Live readings don't flood screen readers. Only a change of the safety verdict, a new alert, or losing and regaining the connection is announced, once.
- Charts have text descriptions: tonight's darkness, moon times and the sky-quality trend.
- With "reduce motion" switched on in your system, animations are switched off.

## Known exceptions

| What | Criterion | Why | Plan |
|---|---|---|---|
| Screen-reader testing with JAWS and TalkBack | — | The reference screen readers are VoiceOver and NVDA; others are best effort | Reports welcome |
| Third-party theme widgets (Material for MkDocs search and navigation) on this site | various | Upstream code. Issues are reported upstream rather than worked around with heavy overrides | Re-check on theme updates |

## Night use

The interface has one dark theme, which is also its night theme. Contrast minimums apply to it; how bright your screen is stays your choice. If a red night-vision theme is ever added, it must meet the same contrast or be listed here as an exception.

## Report a problem

If something doesn't work for you, please [open an issue on GitHub](https://github.com/DeanJ87/SQMeter/issues/new?labels=accessibility&title=Accessibility:%20) with the label **accessibility**. Say what you were trying to do, the page, and your browser and assistive technology if any.

## For contributors

The rules every change must meet, and the manual checklist run before a release, are in [Development → Accessibility](development/accessibility.md).
