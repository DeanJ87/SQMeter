import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { AlertChannelName, AlertRecord } from '../../types';
import { mergeAlertsConfig } from './defaults';
import type { SettingsTabProps } from './context';
import { Note } from '../ui';
import { ActionButton, Field, Group, NumberInput, Requires, ResultNote, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

const AlertsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, dirty, goTo }) => {
  const alerts = mergeAlertsConfig(config.alerts);
  const [testResult, setTestResult] = useState<{ channel: AlertChannelName; type: 'success' | 'error' | 'pending'; text: string } | null>(null);

  const set = (path: string[], value: unknown) => update(['alerts', ...path], value);
  const err = (key: string) => error(`alerts.${key}`);
  const off = !alerts.enabled;

  const fetchRecent = async (): Promise<AlertRecord[]> => {
    const response = await fetch('/api/alerts/recent');
    if (!response.ok) return [];
    const body = await response.json();
    return Array.isArray(body?.alerts) ? body.alerts : [];
  };

  // The device sends the test in the background; follow its delivery
  // status until the channel reports sent / failed / skipped.
  const sendTest = async (channel: AlertChannelName) => {
    setTestResult({ channel, type: 'pending', text: 'Sending...' });
    try {
      const before = (await fetchRecent())[0]?.id ?? 0;
      const response = await fetch(`/api/alerts/test?channel=${channel}`, { method: 'POST' });
      if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        setTestResult({ channel, type: 'error', text: body.error ?? 'Test failed' });
        return;
      }
      const deadline = Date.now() + 30000;
      while (Date.now() < deadline) {
        await new Promise((resolve) => setTimeout(resolve, 1000));
        const record = (await fetchRecent()).find((r) => r.id > before && r.event === 'test');
        const result = record?.channels[channel];
        if (result && result.status !== 'pending') {
          setTestResult(
            result.status === 'sent'
              ? { channel, type: 'success', text: 'Delivered.' }
              : { channel, type: 'error', text: `${result.status === 'skipped' ? 'Skipped' : 'Failed'}: ${result.detail}` }
          );
          return;
        }
      }
      setTestResult({ channel, type: 'error', text: 'No result from the device after 30 s.' });
    } catch {
      setTestResult({ channel, type: 'error', text: 'Could not reach the device' });
    }
  };

  // Render function (not a nested component) so re-renders don't remount it.
  const testButton = (channel: AlertChannelName) => (
    <div class="btn-row">
      <ActionButton onClick={() => sendTest(channel)} disabled={dirty || testResult?.type === 'pending'} title={dirty ? 'Save first' : undefined}>
        Send test
      </ActionButton>
      {testResult?.channel === channel &&
        (testResult.type === 'pending' ? <Note>{testResult.text}</Note> : <ResultNote result={{ type: testResult.type, text: testResult.text }} />)}
    </div>
  );

  const rainReason = !hw.rain.enabled ? 'Rain sensor is off.' : null;
  const dewReason = hw.environment.detected === false ? 'BME280 not detected.' : null;
  const clearReason = hw.irSky.detected === false ? 'MLX90614 not detected.' : null;
  const channelCount = [alerts.pushover.enabled, alerts.ntfy.enabled, alerts.webhook.enabled, alerts.mqtt.enabled].filter(Boolean).length;

  return (
    <>
      <SettingsCard
        id="alerts"
        title="Alerts"
        hint="Push notifications sent by the device itself."
        badge={
          off ? undefined : <StatusBadge
            tone={channelCount === 0 ? 'warn' : 'ok'}
            label={channelCount === 0 ? 'No channels' : `${channelCount} channel${channelCount === 1 ? '' : 's'}`}
          />
        }
      >
        <Toggle label="Send alerts" checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
        {!off && channelCount === 0 && <Requires tone="warn">Turn on a channel below.</Requires>}
      </SettingsCard>

      <SettingsCard title="Notify me when">
        <fieldset class="card-body" disabled={off}>
          <Toggle
            label="Safety changes"
            checked={alerts.onSafetyChange}
            onChange={(v) => set(['onSafetyChange'], v)}
            hint="Unsafe, and safe again. Unsafe alerts list the reasons."
            disabled={off}
          />
          <Toggle
            label="Rain starts or clears"
            checked={alerts.onRain}
            onChange={(v) => set(['onRain'], v)}
            blockedReason={rainReason}
            onFix={() => goTo('sensors', 'rain')}
            disabled={off}
          />
          <Toggle
            label="A sensor fails or recovers"
            checked={alerts.onSensorFault}
            onChange={(v) => set(['onSensorFault'], v)}
            hint="Includes the RG-15 lens fault."
            disabled={off}
          />
          <div class="rule-row">
            <Toggle label="Dew risk" checked={alerts.onDewRisk} onChange={(v) => set(['onDewRisk'], v)} blockedReason={dewReason} disabled={off} hint="Temperature within this margin of the dew point." />
            <NumberInput min={0} max={10} step={0.5} unit="°C" ariaLabel="Dew risk margin" value={alerts.dewRiskMarginC} disabled={off || !alerts.onDewRisk} onChange={(v) => set(['dewRiskMarginC'], v)} />
          </div>
          <div class="rule-row">
            <Toggle label="Skies clear" checked={alerts.onClearSky} onChange={(v) => set(['onClearSky'], v)} blockedReason={clearReason} disabled={off} hint="Cloud cover drops below this." />
            <NumberInput min={0} max={100} step={1} unit="%" ariaLabel="Clear sky cloud cover" value={alerts.clearSkyCloudPercent} disabled={off || !alerts.onClearSky} onChange={(v) => set(['clearSkyCloudPercent'], v)} />
          </div>
          <div class="form-grid">
            <Field label="Cooldown" error={err('cooldownSeconds')} hint="Minimum gap between alerts of the same kind. A change held back is sent when it ends.">
              <NumberInput integer min={0} max={86400} unit="s" value={alerts.cooldownSeconds} disabled={off} onChange={(v) => set(['cooldownSeconds'], v)} />
            </Field>
          </div>
        </fieldset>
      </SettingsCard>

      <SettingsCard title="Channels" hint="Tests use the saved settings and work while alerts are off. Sent alerts appear under the bell in the header.">
        <Group>
          <Toggle label="Pushover" checked={alerts.pushover.enabled} onChange={(v) => set(['pushover', 'enabled'], v)} />
          {alerts.pushover.enabled && (
            <>
              <div class="form-grid">
                <Field label="User key" error={err('pushover.userKey')} hint="Your user key, top of the Pushover dashboard - not the app token or your email.">
                  <TextInput dataField="alerts.pushover.userKey" type="password" value={alerts.pushover.userKey} onInput={(v) => set(['pushover', 'userKey'], v)} />
                </Field>
                <Field label="App token" error={err('pushover.appToken')} hint="Create an application at pushover.net.">
                  <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
                </Field>
                <Field label="Urgent priority" hint="Used for rain, unsafe and sensor faults.">
                  <SelectInput
                    value={String(alerts.pushover.highPriority)}
                    options={[
                      { value: '0', label: 'Normal' },
                      { value: '1', label: 'High' },
                      { value: '2', label: 'Emergency' },
                    ]}
                    onChange={(v) => set(['pushover', 'highPriority'], parseInt(v, 10))}
                  />
                </Field>
                <Field label="Sound">
                  <TextInput value={alerts.pushover.sound} placeholder="Default" onInput={(v) => set(['pushover', 'sound'], v)} />
                </Field>
              </div>
              {testButton('pushover')}
            </>
          )}
        </Group>

        <Group>
          <Toggle label="ntfy" checked={alerts.ntfy.enabled} onChange={(v) => set(['ntfy', 'enabled'], v)} />
          {alerts.ntfy.enabled && (
            <>
              <div class="form-grid">
                <Field label="Server" error={err('ntfy.server')}>
                  <TextInput type="url" value={alerts.ntfy.server} onInput={(v) => set(['ntfy', 'server'], v)} />
                </Field>
                <Field label="Topic" error={err('ntfy.topic')} hint="ntfy.sh topics are public - use a long random name.">
                  <TextInput dataField="alerts.ntfy.topic" value={alerts.ntfy.topic} onInput={(v) => set(['ntfy', 'topic'], v)} />
                </Field>
                <Field label="Token">
                  <TextInput type="password" value={alerts.ntfy.token} placeholder="Optional" onInput={(v) => set(['ntfy', 'token'], v)} />
                </Field>
              </div>
              {testButton('ntfy')}
            </>
          )}
        </Group>

        <Group>
          <Toggle label="Webhook" checked={alerts.webhook.enabled} onChange={(v) => set(['webhook', 'enabled'], v)} hint="POSTs JSON: device, event, title, message, priority, timestamp." />
          {alerts.webhook.enabled && (
            <>
              <div class="form-grid">
                <Field label="URL" error={err('webhook.url')}>
                  <TextInput dataField="alerts.webhook.url" type="url" value={alerts.webhook.url} placeholder="http://homeassistant.local:8123/api/webhook/..." onInput={(v) => set(['webhook', 'url'], v)} />
                </Field>
                <Field label="Authorization header">
                  <TextInput type="password" value={alerts.webhook.authHeader} placeholder="Optional" onInput={(v) => set(['webhook', 'authHeader'], v)} />
                </Field>
              </div>
              {alerts.webhook.url.startsWith('https://') && (
                <Toggle
                  label="Skip certificate checks"
                  checked={alerts.webhook.insecureTls}
                  onChange={(v) => set(['webhook', 'insecureTls'], v)}
                  hint="Only for a self-signed server on your own network."
                />
              )}
              {testButton('webhook')}
            </>
          )}
        </Group>

        <Group aside={alerts.mqtt.enabled && hw.mqtt.enabled && hw.mqtt.connected === false ? <StatusBadge tone="bad" label="Broker not connected" /> : undefined}>
          <Toggle
            label="MQTT"
            checked={alerts.mqtt.enabled}
            onChange={(v) => set(['mqtt', 'enabled'], v)}
            blockedReason={!hw.mqtt.enabled ? 'MQTT is off.' : null}
            onFix={() => goTo('network', 'mqtt')}
            hint={`Publishes to ${config.mqtt.topic}/alerts and a retained ${config.mqtt.topic}/safety.`}
          />
          {alerts.mqtt.enabled && hw.mqtt.enabled && testButton('mqtt')}
        </Group>
      </SettingsCard>

    </>
  );
};

export default AlertsTab;
