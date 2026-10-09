import { describe, expect, it } from 'vitest';
import { describeDarkness } from '../darkness';

const london = { latitude: 51.5, longitude: -0.12 };
const night = new Date('2026-10-08T23:00:00Z');

describe('describeDarkness (spec 005 FR-005, spec 026 A10)', () => {
  it('says unknown when the device has no clock or location', () => {
    const view = describeDarkness({
      sky: { locationSource: 'none', nightKnown: false },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(view).toMatchObject({ dark: 'Unknown', unsaved: false });
    expect(view?.sun).toBeUndefined();
    expect(view?.hint).toMatch(/needs the time and a location/);
  });

  it("uses the device's own decision, not the browser's", () => {
    // The device says not dark at -11.9°, and dark at -13°.
    const notDark = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: false, sunAltitudeDeg: -11.9 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(notDark?.sun).toBe('-11.9°');
    expect(notDark?.dark).not.toMatch(/^Dark/);

    const dark = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: true, sunAltitudeDeg: -30 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(dark?.dark).toMatch(/^Dark/);
    expect(dark?.unsaved).toBe(false);
    expect(dark?.hint).toMatch(/this browser's time zone/);
  });

  it('asks to save when the form limit would change the answer', () => {
    // Device (saved limit -18) says not dark at -15°; the form's -12 would say dark.
    const view = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: false, sunAltitudeDeg: -15 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(view?.unsaved).toBe(true);
  });

  it('still answers without a location for predictions', () => {
    const view = describeDarkness({
      sky: { locationSource: 'gps', nightKnown: true, isNight: true, sunAltitudeDeg: -20 },
      location: null,
      formLimitDeg: -12,
    });
    expect(view).toMatchObject({ sun: '-20.0°', dark: 'Dark now' });
  });
});
