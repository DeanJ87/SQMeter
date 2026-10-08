import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import type { AlertChannelName, AlertRecord } from '../../types';
import { mergeAlertsConfig } from './defaults';
import type { SettingsTabProps } from './context';
import {
  ActionButton,
  Field,
  Group,
  NumberInput,
  Requires,
  SelectInput,
  SettingsCard,
  StatusBadge,
  TextInput,
  Toggle,
} from './controls';

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

const AlertsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, dirty, goTo }) => {
  const alerts = mergeAlertsConfig(config.alerts);
  const [recent, setRecent] = useState<AlertRecord[] | null>(null);
  const [testResult, setTestResult] = useState<{ channel: AlertChannelName; type: 'success' | 'error'; text: string } | null>(null);

  const set = (path: string[], value: unknown) => update(['alerts', ...path], value);
  const err = (key: string) => error(`alerts.${key}`);
  const off = !alerts.enabled;

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
      setTestResult({ channel, type: 'success', text: 'Test queued - check Recent alerts below for delivery.' });
      [1500, 5000, 12000].forEach((delay) => setTimeout(loadRecent, delay));
    } catch {
      setTestResult({ channel, type: 'error', text: 'Could not reach the device' });
    }
  };

  // Render function (not a nested component) so re-renders don't remount it.
  const testButton = (channel: AlertChannelName) => (
    <div class="flex flex-wrap items-center gap-3">
      <ActionButton
        onClick={() => sendTest(channel)}
        disabled={dirty}
        title={dirty ? 'Save first - the test uses the saved settings' : undefined}
      >
        Send test
      </ActionButton>
      {dirty && testResult?.channel !== channel && <span class="text-xs text-gray-500">Save first - tests use the saved settings.</span>}
      {testResult?.channel === channel && (
        <span class={`text-xs ${testResult.type === 'success' ? 'text-green-300' : 'text-red-300'}`}>{testResult.text}</span>
      )}
    </div>
  );

  const rainReason = !hw.rain.enabled ? 'Needs the rain sensor, which is turned off.' : null;
  const dewReason = hw.environment.detected === false ? "Needs the BME280, which wasn't detected." : null;
  const clearReason = hw.irSky.detected === false ? "Needs the MLX90614 IR sensor, which wasn't detected." : null;
  const channelCount = [alerts.pushover.enabled, alerts.ntfy.enabled, alerts.webhook.enabled, alerts.mqtt.enabled].filter(Boolean).length;

  return (
    <>
      <SettingsCard
        id="alerts"
        title="Alerts"
        description="Push notifications sent by the device itself - no N.I.N.A. or PC needed."
        badge={
          <StatusBadge
            tone={off ? 'off' : channelCount === 0 ? 'warn' : 'ok'}
            label={off ? 'Off' : channelCount === 0 ? 'No channels' : `${channelCount} channel${channelCount === 1 ? '' : 's'}`}
          />
        }
      >
        <Toggle label="Send alerts" checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
        {!off && channelCount === 0 && <Requires tone="warn">Turn on at least one channel below, or nothing will be sent.</Requires>}
      </SettingsCard>

      <SettingsCard title="When to alert">
        <fieldset disabled={off} class={off ? 'opacity-50' : ''}>
          <div class="space-y-3">
            <Toggle
              label="Safety changes - unsafe, and safe again"
              checked={alerts.onSafetyChange}
              onChange={(v) => set(['onSafetyChange'], v)}
              hint="Follows the Safety tab's verdict, including the safe delay. Unsafe alerts list the reasons."
            />
            <Toggle
              label="Rain starts and clears"
              checked={alerts.onRain}
              onChange={(v) => set(['onRain'], v)}
              blockedReason={rainReason}
            />
            {rainReason && (
              <button type="button" class="ml-7 -mt-2 text-xs text-cyan-300 hover:underline" onClick={() => goTo('sensors', 'rain')}>
                Set up the rain sensor →
              </button>
            )}
            <Toggle
              label="A sensor stops responding, and recovers"
              checked={alerts.onSensorFault}
              onChange={(v) => set(['onSensorFault'], v)}
              hint="Includes the RG-15 lens-fault flag."
            />
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 pt-2">
              <div class="space-y-2">
                <Toggle label="Dew risk" checked={alerts.onDewRisk} onChange={(v) => set(['onDewRisk'], v)} blockedReason={dewReason} />
                <Field label="Temperature within (°C) of dew point" class="ml-7">
                  <NumberInput min={0} max={10} step={0.5} value={alerts.dewRiskMarginC} disabled={!alerts.onDewRisk} onChange={(v) => set(['dewRiskMarginC'], v)} />
                </Field>
              </div>
              <div class="space-y-2">
                <Toggle label="Skies clear" checked={alerts.onClearSky} onChange={(v) => set(['onClearSky'], v)} blockedReason={clearReason} />
                <Field label="Cloud cover below (%)" class="ml-7">
                  <NumberInput min={0} max={100} step={1} value={alerts.clearSkyCloudPercent} disabled={!alerts.onClearSky} onChange={(v) => set(['clearSkyCloudPercent'], v)} />
                </Field>
              </div>
            </div>
            <Field
              label="Cooldown (seconds)"
              error={err('cooldownSeconds')}
              hint="Minimum gap between alerts of the same kind. A change held back by the cooldown is sent when it ends, so the latest alert always matches reality."
            >
              <div class="max-w-xs">
                <NumberInput integer min={0} max={86400} value={alerts.cooldownSeconds} onChange={(v) => set(['cooldownSeconds'], v)} />
              </div>
            </Field>
          </div>
        </fieldset>
      </SettingsCard>

      <SettingsCard title="Where to send them" description="Tests use the saved settings and work even while alerts are off.">
        <Group title="Pushover" aside={<StatusBadge tone={alerts.pushover.enabled ? 'ok' : 'off'} label={alerts.pushover.enabled ? 'On' : 'Off'} />}>
          <Toggle label="Pushover" checked={alerts.pushover.enabled} onChange={(v) => set(['pushover', 'enabled'], v)} />
          {alerts.pushover.enabled && (
            <div class="ml-7 space-y-3">
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <Field label="User key" error={err('pushover.userKey')}>
                  <TextInput dataField="alerts.pushover.userKey" type="password" value={alerts.pushover.userKey} onInput={(v) => set(['pushover', 'userKey'], v)} />
                </Field>
                <Field label="Application API token" hint="Create an application at pushover.net.">
                  <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
                </Field>
                <Field label="Priority for urgent alerts" hint="Rain, unsafe and sensor faults. Others use normal priority.">
                  <SelectInput
                    value={String(alerts.pushover.highPriority)}
                    options={[
                      { value: '0', label: 'Normal' },
                      { value: '1', label: 'High (bypasses quiet hours)' },
                      { value: '2', label: 'Emergency (repeats until acknowledged)' },
                    ]}
                    onChange={(v) => set(['pushover', 'highPriority'], parseInt(v, 10))}
                  />
                </Field>
                <Field label="Sound (optional)">
                  <TextInput value={alerts.pushover.sound} placeholder="e.g. siren" onInput={(v) => set(['pushover', 'sound'], v)} />
                </Field>
              </div>
              {testButton('pushover')}
            </div>
          )}
        </Group>

        <Group title="ntfy" aside={<StatusBadge tone={alerts.ntfy.enabled ? 'ok' : 'off'} label={alerts.ntfy.enabled ? 'On' : 'Off'} />}>
          <Toggle label="ntfy" checked={alerts.ntfy.enabled} onChange={(v) => set(['ntfy', 'enabled'], v)} />
          {alerts.ntfy.enabled && (
            <div class="ml-7 space-y-3">
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <Field label="Server" error={err('ntfy.server')}>
                  <TextInput type="url" value={alerts.ntfy.server} onInput={(v) => set(['ntfy', 'server'], v)} />
                </Field>
                <Field label="Topic" error={err('ntfy.topic')} hint="ntfy.sh topics are public - use a long random name.">
                  <TextInput dataField="alerts.ntfy.topic" value={alerts.ntfy.topic} onInput={(v) => set(['ntfy', 'topic'], v)} />
                </Field>
                <Field label="Access token (optional)">
                  <TextInput type="password" value={alerts.ntfy.token} onInput={(v) => set(['ntfy', 'token'], v)} />
                </Field>
              </div>
              {testButton('ntfy')}
            </div>
          )}
        </Group>

        <Group title="Webhook" aside={<StatusBadge tone={alerts.webhook.enabled ? 'ok' : 'off'} label={alerts.webhook.enabled ? 'On' : 'Off'} />}>
          <Toggle label="Webhook" checked={alerts.webhook.enabled} onChange={(v) => set(['webhook', 'enabled'], v)} />
          {alerts.webhook.enabled && (
            <div class="ml-7 space-y-3">
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <Field label="URL" error={err('webhook.url')} hint='POSTs JSON: {"device","event","title","message","priority","timestamp"}'>
                  <TextInput
                    dataField="alerts.webhook.url"
                    type="url"
                    value={alerts.webhook.url}
                    placeholder="http://homeassistant.local:8123/api/webhook/..."
                    onInput={(v) => set(['webhook', 'url'], v)}
                  />
                </Field>
                <Field label="Authorization header (optional)">
                  <TextInput type="password" value={alerts.webhook.authHeader} placeholder="Bearer ..." onInput={(v) => set(['webhook', 'authHeader'], v)} />
                </Field>
              </div>
              <Toggle
                label="Skip TLS certificate checks"
                checked={alerts.webhook.insecureTls}
                onChange={(v) => set(['webhook', 'insecureTls'], v)}
                hint="Only for a self-signed server on your own network."
                disabled={!alerts.webhook.url.startsWith('https://')}
              />
              {testButton('webhook')}
            </div>
          )}
        </Group>

        <Group
          title="MQTT"
          aside={
            alerts.mqtt.enabled && hw.mqtt.enabled && hw.mqtt.connected === false
              ? <StatusBadge tone="bad" label="Broker not connected" />
              : <StatusBadge tone={alerts.mqtt.enabled && hw.mqtt.enabled ? 'ok' : 'off'} label={alerts.mqtt.enabled && hw.mqtt.enabled ? 'On' : 'Off'} />
          }
        >
          <Toggle
            label="MQTT"
            checked={alerts.mqtt.enabled}
            onChange={(v) => set(['mqtt', 'enabled'], v)}
            blockedReason={!hw.mqtt.enabled ? 'MQTT is turned off on the Network tab.' : null}
            hint={hw.mqtt.enabled ? `Publishes to ${config.mqtt.topic}/alerts and a retained ${config.mqtt.topic}/safety.` : undefined}
          />
          {!hw.mqtt.enabled && (
            <button type="button" class="ml-7 -mt-2 text-xs text-cyan-300 hover:underline" onClick={() => goTo('network', 'mqtt')}>
              Set up MQTT →
            </button>
          )}
          {alerts.mqtt.enabled && hw.mqtt.enabled && <div class="ml-7">{testButton('mqtt')}</div>}
        </Group>
      </SettingsCard>

      <SettingsCard
        title="Recent alerts"
        badge={
          <button type="button" class="text-xs text-cyan-300 hover:underline" onClick={loadRecent}>
            Refresh
          </button>
        }
      >
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
      </SettingsCard>
    </>
  );
};

export default AlertsTab;
