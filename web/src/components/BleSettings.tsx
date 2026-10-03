import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import type { Config, SystemStatus } from '../types';

interface Props {
  config: Config;
  updateConfig: (path: string[], value: unknown) => void;
}

const BleSettings: FunctionalComponent<Props> = ({ config, updateConfig }) => {
  const [ble, setBle] = useState<SystemStatus['ble'] | null>(null);

  useEffect(() => {
    fetch('/api/status')
      .then((response) => (response.ok ? response.json() : null))
      .then((status: SystemStatus | null) => setBle(status?.ble ?? { available: false, active: false, clients: 0 }))
      .catch(() => setBle({ available: false, active: false, clients: 0 }));
  }, []);

  const enabled = config.ble?.enabled ?? false;

  return (
    <section id="ble" class="bg-gray-800 rounded-lg p-6 border border-gray-700 scroll-mt-4">
      <h2 class="text-xl font-semibold text-white mb-2">Bluetooth (BLE)</h2>
      {ble === null && <p class="text-sm text-gray-500">Loading...</p>}
      {ble && !ble.available && (
        <p class="text-sm text-gray-400">
          This firmware is the standard build, which has no room for Bluetooth. Flash the <code>esp32dev-ble</code> build over
          USB to broadcast safety and rain state over BLE - see the BLE guide in the docs.
        </p>
      )}
      {ble?.available && (
        <div class="space-y-3">
          <p class="text-sm text-gray-400">
            Broadcasts the safety verdict and rain state in the advertisement, and serves a read-only GATT service (safety, rain,
            latest alert, sensor summary) with notifications. Changes take effect after a restart.
          </p>
          <label class="flex items-center gap-3">
            <input
              type="checkbox"
              checked={enabled}
              onChange={(e) => updateConfig(['ble', 'enabled'], (e.target as HTMLInputElement).checked)}
            />
            <span class="text-white">Enable Bluetooth</span>
          </label>
          <p class="text-xs text-gray-500">
            Status: {ble.active ? `advertising, ${ble.clients} client${ble.clients === 1 ? '' : 's'} connected` : 'off'}
            {ble.active !== enabled && ' - restart to apply'}
          </p>
          <p class="text-xs text-amber-300">
            WiFi and Bluetooth share one radio: with Bluetooth on, the web UI and Alpaca respond noticeably slower (around
            0.3-3.6 s instead of under 0.2 s).
          </p>
          <p class="text-xs text-gray-500">Alerts appear on the BLE alert characteristic when "Enable alerts" is on above.</p>
        </div>
      )}
    </section>
  );
};

export default BleSettings;
