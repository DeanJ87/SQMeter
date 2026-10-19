import { describe, it, expect } from 'vitest';
import { getConfigValidationErrors } from '../validation/configSchema';
import { mockConfig } from '../mocks/data';
import type { Config, DisplayConfig } from '../types';

// Spec 031: the display settings, with the device's messages (D-45 for the wake button pin).
describe('display settings', () => {
  const withDisplay = (display: Partial<DisplayConfig>, extra: Partial<Config> = {}) =>
    getConfigValidationErrors({ ...mockConfig, ...extra, display: { ...mockConfig.display!, ...display } });

  it('accepts the defaults and every page once', () => {
    expect(withDisplay({})).toEqual({});
    expect(withDisplay({ pages: ['network', 'sky', 'environment', 'cloud', 'safety', 'gps', 'battery'] })).toEqual({});
  });
  it('rejects no pages or a page twice', () => {
    expect(withDisplay({ pages: [] })['display.pages']).toBe('Choose 1 to 7 different pages');
    expect(withDisplay({ pages: ['sky', 'sky'] })['display.pages']).toBe('Choose 1 to 7 different pages');
  });
  it('allows never (0) or 10 seconds to 30 minutes', () => {
    expect(withDisplay({ timeoutSeconds: 0 })).toEqual({});
    expect(withDisplay({ timeoutSeconds: 5 })['display.timeoutSeconds']).toBe('10 seconds to 30 minutes, or never');
  });
  it('D-45: wake button pin is none or a free valid GPIO', () => {
    expect(withDisplay({ buttonPin: 255 })).toEqual({});
    expect(withDisplay({ buttonPin: 33 })).toEqual({});
    expect(withDisplay({ buttonPin: 7 })['display.buttonPin']).toBe('Display: wake button pin is not a valid GPIO');
    expect(withDisplay({ buttonPin: 21 })['display.buttonPin']).toBe('Display: wake button pin is already in use');
    // mockConfig has the anemometer on 27 and the vane on 35.
    expect(withDisplay({ buttonPin: 35 })['display.buttonPin']).toBe('Display: wake button pin is already in use');
    // A clash doesn't matter while the display is off.
    expect(withDisplay({ enabled: false, buttonPin: 21 })).toEqual({});
  });
});
