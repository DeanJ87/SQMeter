import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import type { AlertChannelName, AlertRecord, AlertsConfig, Config } from '../types';

export const defaultAlertsConfig: AlertsConfig = {
  enabled: false,
  onSafetyChange: true,
  onRain: true,
  onSensorFault: true,
  onDewRisk: false,
  dewRiskMarginC: 2,
  onClearSky: false,
  clearSkyCloudPercent: 20,
  cooldownSeconds: 300,
  pushover: { enabled: false, userKey: '', appToken: '', highPriority: 1, sound: '' },
  ntfy: { enabled: false, server: 'https://ntfy.sh', topic: '', token: '' },
  webhook: { enabled: false, url: '', authHeader: '', insecureTls: false },
  mqtt: { enabled: false },
};

// Fill in fields missing from configs saved by older firmware.
export const mergeAlertsConfig = (source?: Partial<AlertsConfig>): AlertsConfig => ({
  ...defaultAlertsConfig,
  ...source,
  pushover: { ...defaultAlertsConfig.pushover, ...source?.pushover },
  ntfy: { ...defaultAlertsConfig.ntfy, ...source?.ntfy },
  webhook: { ...defaultAlertsConfig.webhook, ...source?.webhook },
  mqtt: { ...defaultAlertsConfig.mqtt, ...source?.mqtt },
});

interface Props {
  config: Config;
  updateConfig: (path: string[], value: unknown) => void;
  validationErrors: Record<string, string>;
}

const inputClass =
  'w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500 disabled:opacity-50';

const statusTone: Record<string, string> = {
  sent: 'text-green-300',
  pending: 'text-gray-400',
  failed: 'text-red-300',
  skipped: 'text-amber-300',
};

const formatAge = (seconds: number) => {
  if (seconds < 60) return `${seconds}s ago`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
  return `${Math.floor(seconds / 86400)}d ago`;
};

const Checkbox: FunctionalComponent<{ label: string; checked: boolean; onChange: (checked: boolean) => void; hint?: string }> = ({
  label,
  checked,
  onChange,
  hint,
}) => (
  <div>
    <label class="flex items-center gap-3">
      <input type="checkbox" checked={checked} onChange={(e) => onChange((e.target as HTMLInputElement).checked)} />
      <span class="text-white">{label}</span>
    </label>
    {hint && <p class="mt-1 ml-7 text-xs text-gray-500">{hint}</p>}
  </div>
);

const AlertsSettings: FunctionalComponent<Props> = ({ config, updateConfig, validationErrors }) => {
  const alerts = mergeAlertsConfig(config.alerts);
  const [recent, setRecent] = useState<AlertRecord[] | null>(null);
  const [testResult, setTestResult] = useState<{ channel: AlertChannelName; type: 'success' | 'error'; text: string } | null>(null);

  const set = (path: string[], value: unknown) => updateConfig(['alerts', ...path], value);
  const error = (key: string) => validationErrors[`alerts.${key}`];

  const loadRecent = () =>
    fetch('/api/alerts/recent')
      .then((response) => (response.ok ? response.json() : []))
      .then((data: AlertRecord[]) => setRecent(Array.isArray(data) ? data : []))
      .catch(() => setRecent([]));

  useEffect(() => {
    loadRecent();
  }, []);

  const sendTest = async (channel: AlertChannelName) => {
    setTestResult(null);
    try {
      const response = await fetch(`/api/alerts/test?channel=${channel}`, { method: 'POST' });
      const result = await response.json().catch(() => ({}));
      if (!response.ok) {
        setTestResult({ channel, type: 'error', text: result.error ?? 'Test failed' });
        return;
      }
      setTestResult({ channel, type: 'success', text: 'Test queued - delivery status appears under Recent alerts.' });
      // HTTPS delivery happens in the background; refresh as it lands.
      [1500, 5000, 12000].forEach((delay) => setTimeout(loadRecent, delay));
    } catch {
      setTestResult({ channel, type: 'error', text: 'Could not reach the device' });
    }
  };

  const TestButton: FunctionalComponent<{ channel: AlertChannelName; enabled: boolean }> = ({ channel, enabled }) => (
    <div class="flex flex-wrap items-center gap-3">
      <button
        type="button"
        onClick={() => sendTest(channel)}
        disabled={!enabled}
        class="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 disabled:opacity-50 text-white rounded-lg"
      >
        Send test
      </button>
      {testResult?.channel === channel && (
        <span class={`text-xs ${testResult.type === 'success' ? 'text-green-300' : 'text-red-300'}`}>{testResult.text}</span>
      )}
    </div>
  );

  return (
    <section id="alerts" class="bg-gray-800 rounded-lg p-6 border border-gray-700 scroll-mt-4">
      <h2 class="text-xl font-semibold text-white mb-2">Alerts</h2>
      <p class="text-sm text-gray-400 mb-4">
        Push notifications straight from the device - no N.I.N.A. or PC needed. Save settings before sending a test.
      </p>

      <div class="space-y-4">
        <Checkbox label="Enable alerts" checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />

        <div class="border-t border-gray-700 pt-4 space-y-3">
          <h3 class="text-sm font-semibold text-gray-200">Notify me when</h3>
          <Checkbox
            label="Safety changes (unsafe / safe again)"
            checked={alerts.onSafetyChange}
            onChange={(v) => set(['onSafetyChange'], v)}
            hint="Follows the SafetyMonitor verdict, including the safe delay. Unsafe alerts list the reasons."
          />
          <Checkbox label="Rain starts / clears" checked={alerts.onRain} onChange={(v) => set(['onRain'], v)} hint="Needs the RG-15 rain sensor." />
          <Checkbox
            label="A sensor goes offline / recovers"
            checked={alerts.onSensorFault}
            onChange={(v) => set(['onSensorFault'], v)}
            hint="Includes the RG-15 lens-fault flag."
          />
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <Checkbox label="Dew risk" checked={alerts.onDewRisk} onChange={(v) => set(['onDewRisk'], v)} />
              <label class="block mt-2 text-xs text-gray-400">Temperature within this many °C of the dew point</label>
              <input
                type="number"
                class={inputClass}
                value={alerts.dewRiskMarginC}
                min="0"
                max="10"
                step="0.5"
                disabled={!alerts.onDewRisk}
                onChange={(e) => set(['dewRiskMarginC'], parseFloat((e.target as HTMLInputElement).value))}
              />
            </div>
            <div>
              <Checkbox label="Skies clear" checked={alerts.onClearSky} onChange={(v) => set(['onClearSky'], v)} />
              <label class="block mt-2 text-xs text-gray-400">Cloud cover below (%)</label>
              <input
                type="number"
                class={inputClass}
                value={alerts.clearSkyCloudPercent}
                min="0"
                max="100"
                step="1"
                disabled={!alerts.onClearSky}
                onChange={(e) => set(['clearSkyCloudPercent'], parseFloat((e.target as HTMLInputElement).value))}
              />
            </div>
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">Cooldown (seconds)</label>
            <input
              type="number"
              class={inputClass}
              value={alerts.cooldownSeconds}
              min="0"
              max="86400"
              onChange={(e) => set(['cooldownSeconds'], parseInt((e.target as HTMLInputElement).value, 10))}
            />
            <p class="mt-1 text-xs text-gray-500">
              Minimum time between alerts of the same kind. A change held back by the cooldown is still sent once it ends, so the
              latest alert always matches reality.
            </p>
          </div>
        </div>

        {/* Pushover */}
        <div class="border-t border-gray-700 pt-4 space-y-3">
          <Checkbox label="Pushover" checked={alerts.pushover.enabled} onChange={(v) => set(['pushover', 'enabled'], v)} />
          {alerts.pushover.enabled && (
            <>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">User key</label>
                  <input
                    type="password"
                    name="alerts.pushover.userKey"
                    class={inputClass}
                    value={alerts.pushover.userKey}
                    autocomplete="off"
                    onInput={(e) => set(['pushover', 'userKey'], (e.target as HTMLInputElement).value)}
                  />
                  {error('pushover.userKey') && <p class="mt-1 text-xs text-red-400">{error('pushover.userKey')}</p>}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Application API token</label>
                  <input
                    type="password"
                    class={inputClass}
                    value={alerts.pushover.appToken}
                    autocomplete="off"
                    onInput={(e) => set(['pushover', 'appToken'], (e.target as HTMLInputElement).value)}
                  />
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Priority for urgent alerts</label>
                  <select
                    class={inputClass}
                    value={String(alerts.pushover.highPriority)}
                    onChange={(e) => set(['pushover', 'highPriority'], parseInt((e.target as HTMLSelectElement).value, 10))}
                  >
                    <option value="0">Normal</option>
                    <option value="1">High (bypasses quiet hours)</option>
                    <option value="2">Emergency (repeats until acknowledged)</option>
                  </select>
                  <p class="mt-1 text-xs text-gray-500">Used for rain, unsafe and sensor-fault alerts. Others are sent at normal priority.</p>
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Sound (optional)</label>
                  <input
                    type="text"
                    class={inputClass}
                    value={alerts.pushover.sound}
                    placeholder="e.g. siren"
                    onInput={(e) => set(['pushover', 'sound'], (e.target as HTMLInputElement).value)}
                  />
                </div>
              </div>
              <p class="text-xs text-gray-500">Create an application at pushover.net to get an API token; your user key is on the Pushover dashboard.</p>
              <TestButton channel="pushover" enabled={alerts.pushover.enabled} />
            </>
          )}
        </div>

        {/* ntfy */}
        <div class="border-t border-gray-700 pt-4 space-y-3">
          <Checkbox label="ntfy" checked={alerts.ntfy.enabled} onChange={(v) => set(['ntfy', 'enabled'], v)} />
          {alerts.ntfy.enabled && (
            <>
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Server</label>
                  <input
                    type="url"
                    class={inputClass}
                    value={alerts.ntfy.server}
                    onInput={(e) => set(['ntfy', 'server'], (e.target as HTMLInputElement).value)}
                  />
                  {error('ntfy.server') && <p class="mt-1 text-xs text-red-400">{error('ntfy.server')}</p>}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Topic</label>
                  <input
                    type="text"
                    name="alerts.ntfy.topic"
                    class={inputClass}
                    value={alerts.ntfy.topic}
                    onInput={(e) => set(['ntfy', 'topic'], (e.target as HTMLInputElement).value)}
                  />
                  {error('ntfy.topic') && <p class="mt-1 text-xs text-red-400">{error('ntfy.topic')}</p>}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Access token (optional)</label>
                  <input
                    type="password"
                    class={inputClass}
                    value={alerts.ntfy.token}
                    autocomplete="off"
                    onInput={(e) => set(['ntfy', 'token'], (e.target as HTMLInputElement).value)}
                  />
                </div>
              </div>
              <p class="text-xs text-gray-500">Topics on ntfy.sh are public - pick a long, unguessable name, then subscribe to it in the ntfy app.</p>
              <TestButton channel="ntfy" enabled={alerts.ntfy.enabled} />
            </>
          )}
        </div>

        {/* Webhook */}
        <div class="border-t border-gray-700 pt-4 space-y-3">
          <Checkbox label="Webhook" checked={alerts.webhook.enabled} onChange={(v) => set(['webhook', 'enabled'], v)} />
          {alerts.webhook.enabled && (
            <>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">URL</label>
                  <input
                    type="url"
                    name="alerts.webhook.url"
                    class={inputClass}
                    value={alerts.webhook.url}
                    placeholder="http://homeassistant.local:8123/api/webhook/..."
                    onInput={(e) => set(['webhook', 'url'], (e.target as HTMLInputElement).value)}
                  />
                  {error('webhook.url') && <p class="mt-1 text-xs text-red-400">{error('webhook.url')}</p>}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">Authorization header (optional)</label>
                  <input
                    type="password"
                    class={inputClass}
                    value={alerts.webhook.authHeader}
                    placeholder="Bearer ..."
                    autocomplete="off"
                    onInput={(e) => set(['webhook', 'authHeader'], (e.target as HTMLInputElement).value)}
                  />
                </div>
              </div>
              <Checkbox
                label="Skip TLS certificate checks"
                checked={alerts.webhook.insecureTls}
                onChange={(v) => set(['webhook', 'insecureTls'], v)}
                hint="Only for a self-signed server on your own network. Anyone able to intercept the connection could read or spoof alerts."
              />
              <p class="text-xs text-gray-500">
                POSTs JSON: <code>{'{"device","event","title","message","priority","timestamp"}'}</code>.
              </p>
              <TestButton channel="webhook" enabled={alerts.webhook.enabled} />
            </>
          )}
        </div>

        {/* MQTT */}
        <div class="border-t border-gray-700 pt-4 space-y-3">
          <Checkbox
            label="MQTT"
            checked={alerts.mqtt.enabled}
            onChange={(v) => set(['mqtt', 'enabled'], v)}
            hint={`Publishes alerts to ${config.mqtt.topic || '<topic>'}/alerts and the retained verdict to ${config.mqtt.topic || '<topic>'}/safety, using the MQTT settings above.`}
          />
          {alerts.mqtt.enabled && !config.mqtt.enabled && (
            <p class="text-xs text-amber-300">MQTT itself is disabled above, so nothing will be published.</p>
          )}
          {alerts.mqtt.enabled && <TestButton channel="mqtt" enabled={alerts.mqtt.enabled} />}
        </div>

        {/* Recent alerts */}
        <div class="border-t border-gray-700 pt-4">
          <div class="flex items-center justify-between mb-2">
            <h3 class="text-sm font-semibold text-gray-200">Recent alerts</h3>
            <button type="button" class="text-xs text-cyan-300 hover:underline" onClick={loadRecent}>
              Refresh
            </button>
          </div>
          {recent === null && <p class="text-sm text-gray-500">Loading...</p>}
          {recent?.length === 0 && <p class="text-sm text-gray-500">No alerts since the device started.</p>}
          {recent && recent.length > 0 && (
            <ul class="space-y-2" aria-label="Recent alerts">
              {recent.map((record) => (
                <li key={record.id} class="p-3 bg-gray-900/60 rounded-lg">
                  <div class="flex flex-wrap justify-between gap-2">
                    <span class="text-sm text-white font-medium">{record.title}</span>
                    <span class="text-xs text-gray-500">{formatAge(record.ageSeconds)}</span>
                  </div>
                  <p class="text-xs text-gray-400 mt-1">{record.message}</p>
                  <div class="flex flex-wrap gap-3 mt-2">
                    {Object.entries(record.channels).map(([channel, result]) => (
                      <span key={channel} class={`text-xs ${statusTone[result?.status ?? 'pending']}`} title={result?.detail}>
                        {channel}: {result?.status}
                        {result?.status === 'failed' && result.detail ? ` (${result.detail})` : ''}
                      </span>
                    ))}
                  </div>
                </li>
              ))}
            </ul>
          )}
        </div>
      </div>
    </section>
  );
};

export default AlertsSettings;
