import { FunctionalComponent } from 'preact';
import type { Config, WindConfig } from '../types';

export const defaultWindConfig: WindConfig = {
  enabled: false,
  speedPin: 27,
  directionEnabled: false,
  directionPin: 35,
  kmhPerHz: 2.4,
  directionOffsetDeg: 0,
  vanePullupOhms: 10000,
};

const PRESETS = [
  { label: 'Misol / Argent / SparkFun (2.4 km/h per Hz)', value: 2.4 },
  { label: 'Davis 6410 (3.621 km/h per Hz)', value: 3.621 },
];

interface Props {
  config: Config;
  updateConfig: (path: string[], value: unknown) => void;
  validationErrors: Record<string, string>;
}

const inputClass =
  'w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500 disabled:opacity-50';

const WindSettings: FunctionalComponent<Props> = ({ config, updateConfig, validationErrors }) => {
  const wind = { ...defaultWindConfig, ...config.wind };
  const set = (key: keyof WindConfig, value: unknown) => updateConfig(['wind', key], value);
  const error = (key: string) => validationErrors[`wind.${key}`];
  const preset = PRESETS.find((p) => Math.abs(p.value - wind.kmhPerHz) < 0.0005)?.value;

  return (
    <section id="wind" class="bg-gray-800 rounded-lg p-6 border border-gray-700 scroll-mt-4">
      <h2 class="text-xl font-semibold text-white mb-2">Wind (anemometer)</h2>
      <p class="text-sm text-gray-400 mb-4">
        Reed-switch cup anemometer and optional resistor-ladder vane. Feeds ObservingConditions wind speed, gust and direction,
        and the wind safety limits.
      </p>
      <div class="space-y-4">
        <label class="flex items-center gap-3">
          <input type="checkbox" checked={wind.enabled} onChange={(e) => set('enabled', (e.target as HTMLInputElement).checked)} />
          <span class="text-white">Enable anemometer</span>
        </label>

        {wind.enabled && (
          <>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">Anemometer GPIO</label>
                <input
                  type="number"
                  name="wind.speedPin"
                  class={inputClass}
                  value={wind.speedPin}
                  onChange={(e) => set('speedPin', parseInt((e.target as HTMLInputElement).value, 10))}
                />
                {error('speedPin') && <p class="mt-1 text-xs text-red-400">{error('speedPin')}</p>}
                <p class="mt-1 text-xs text-gray-500">Switch to GND; internal pull-up is used. Default GPIO 27.</p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">Anemometer type</label>
                <select
                  class={inputClass}
                  value={preset !== undefined ? String(preset) : 'custom'}
                  onChange={(e) => {
                    const value = (e.target as HTMLSelectElement).value;
                    if (value !== 'custom') set('kmhPerHz', parseFloat(value));
                  }}
                >
                  {PRESETS.map((p) => (
                    <option key={p.value} value={String(p.value)}>
                      {p.label}
                    </option>
                  ))}
                  <option value="custom">Custom</option>
                </select>
                <input
                  type="number"
                  class={`${inputClass} mt-2`}
                  value={wind.kmhPerHz}
                  step="0.001"
                  min="0.001"
                  max="20"
                  aria-label="km/h per Hz"
                  onChange={(e) => set('kmhPerHz', parseFloat((e.target as HTMLInputElement).value))}
                />
                <p class="mt-1 text-xs text-gray-500">Wind speed in km/h for one switch closure per second.</p>
              </div>
            </div>

            <div class="border-t border-gray-700 pt-4 space-y-3">
              <label class="flex items-center gap-3">
                <input
                  type="checkbox"
                  checked={wind.directionEnabled}
                  onChange={(e) => set('directionEnabled', (e.target as HTMLInputElement).checked)}
                />
                <span class="text-white">Wind vane</span>
              </label>
              {wind.directionEnabled && (
                <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                  <div>
                    <label class="block text-sm font-medium text-gray-300 mb-2">Vane GPIO (ADC1)</label>
                    <input
                      type="number"
                      name="wind.directionPin"
                      class={inputClass}
                      value={wind.directionPin}
                      min="32"
                      max="39"
                      onChange={(e) => set('directionPin', parseInt((e.target as HTMLInputElement).value, 10))}
                    />
                    {error('directionPin') && <p class="mt-1 text-xs text-red-400">{error('directionPin')}</p>}
                    <p class="mt-1 text-xs text-gray-500">GPIO 32-39 only - ADC2 doesn't work while WiFi is on.</p>
                  </div>
                  <div>
                    <label class="block text-sm font-medium text-gray-300 mb-2">Pull-up resistor (Ω)</label>
                    <input
                      type="number"
                      class={inputClass}
                      value={wind.vanePullupOhms}
                      step="100"
                      onChange={(e) => set('vanePullupOhms', parseFloat((e.target as HTMLInputElement).value))}
                    />
                    <p class="mt-1 text-xs text-gray-500">From the vane pin to 3.3 V (default 10 kΩ).</p>
                  </div>
                  <div>
                    <label class="block text-sm font-medium text-gray-300 mb-2">North offset (°)</label>
                    <input
                      type="number"
                      class={inputClass}
                      value={wind.directionOffsetDeg}
                      step="1"
                      onChange={(e) => set('directionOffsetDeg', parseFloat((e.target as HTMLInputElement).value))}
                    />
                    <p class="mt-1 text-xs text-gray-500">Added to the vane reading if it isn't mounted pointing north.</p>
                  </div>
                </div>
              )}
            </div>
          </>
        )}
      </div>
    </section>
  );
};

export default WindSettings;
