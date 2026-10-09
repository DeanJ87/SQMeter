import { describe, expect, it } from 'vitest';
import { describeDarkness } from '../darkness';

const london = { latitude: 51.5, longitude: -0.12 };
const night = new Date('2026-10-08T23:00:00Z');

describe('describeDarkness (spec 005 FR-005)', () => {
  it('says unknown when the device has no clock or location', () => {
    const note = describeDarkness({ sky: { locationSource: 'none', nightKnown: false }, location: london, formLimitDeg: -12, deviceNow: night });
    expect(note).toMatch(/unknown/);
    expect(note).not.toMatch(/Sun at/);
  });

  it("uses the device's own decision, not the browser's", () => {
    // The device says not dark at -11.9°, even though the browser would agree;
    // and dark at -13°.
    const notDark = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: false, sunAltitudeDeg: -11.9 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(notDark).toMatch(/Sun at -11\.9° now \(device\)/);
    expect(notDark).not.toMatch(/ - dark[ .]/);

    const dark = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: true, sunAltitudeDeg: -30 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(dark).toMatch(/ - dark/);
    expect(dark).not.toMatch(/Save to apply/);
  });

  it('asks to save when the form limit would change the answer', () => {
    // Device (saved limit -18) says not dark at -15°; the form's -12 would say dark.
    const note = describeDarkness({
      sky: { locationSource: 'manual', nightKnown: true, isNight: false, sunAltitudeDeg: -15 },
      location: london,
      formLimitDeg: -12,
      deviceNow: night,
    });
    expect(note).toMatch(/Save to apply the new limit/);
  });

  it('still answers without a location for predictions', () => {
    const note = describeDarkness({
      sky: { locationSource: 'gps', nightKnown: true, isNight: true, sunAltitudeDeg: -20 },
      location: null,
      formLimitDeg: -12,
    });
    expect(note).toBe('Sun at -20.0° now (device) - dark.');
  });
});
