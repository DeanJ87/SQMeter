import { FunctionalComponent } from 'preact';
import type { AlertChannelName } from '../../types';
import { isPending, soundOptions, type AlertsView } from './alertsShared';
import { AlertTestNote } from './AlertTemplateEditor';
import { ActionButton, DepToggle, Field, Group, SelectInput, SettingsCard, TextInput, Toggle } from './controls';
import { t } from '../../i18n';

// Send test, for a channel that is on (tests use the saved settings).
const TestButton: FunctionalComponent<{ view: AlertsView; channel: AlertChannelName }> = ({ view, channel }) => {
  const entry = view.dep(`alerts.${channel}.enabled`);
  if (entry.state === 'off' || entry.state === 'inactive') return null;
  return (
    <div class="btn-row">
      <ActionButton
        onClick={() => view.tester.sendTest(channel)}
        disabled={view.dirty || isPending(view.tester.testResult)}
        title={view.dirty ? t('settings.alerts.saveFirst') : undefined}
      >
        {t('settings.alerts.sendTest')}
      </ActionButton>
      <AlertTestNote view={view} target={channel} />
    </div>
  );
};

const PushoverGroup: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, set, err, dep, fix } = view;
  return (
    <Group>
      <DepToggle
        entry={dep('alerts.pushover.enabled')}
        onFix={fix}
        label="Pushover"
        checked={alerts.pushover.enabled}
        onChange={(v) => set(['pushover', 'enabled'], v)}
      />
      {alerts.pushover.enabled && (
        <>
          <div class="form-grid">
            <Field label={t('settings.alerts.userKey')} error={err('pushover.userKey')} hint={t('settings.alerts.yourUserKeyTopOf')}>
              <TextInput
                dataField="alerts.pushover.userKey"
                type="password"
                value={alerts.pushover.userKey}
                onInput={(v) => set(['pushover', 'userKey'], v)}
              />
            </Field>
            <Field
              label={t('settings.alerts.appToken')}
              error={err('pushover.appToken')}
              hint={t('settings.alerts.createAnApplicationAtPushover')}
            >
              <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
            </Field>
            <Field label={t('settings.alerts.defaultSound')} hint={t('settings.alerts.usedWhereAnEventS')}>
              <SelectInput
                value={alerts.pushover.sound}
                options={soundOptions(alerts.pushover.sound, t('settings.alerts.pushoverDefault'))}
                onChange={(v) => set(['pushover', 'sound'], v)}
              />
            </Field>
          </div>
          <TestButton view={view} channel="pushover" />
        </>
      )}
    </Group>
  );
};

const NtfyGroup: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, set, err, dep, fix } = view;
  return (
    <Group>
      <DepToggle
        entry={dep('alerts.ntfy.enabled')}
        onFix={fix}
        label="ntfy"
        checked={alerts.ntfy.enabled}
        onChange={(v) => set(['ntfy', 'enabled'], v)}
      />
      {alerts.ntfy.enabled && (
        <>
          <div class="form-grid">
            <Field label={t('settings.alerts.server')} error={err('ntfy.server')}>
              <TextInput type="url" value={alerts.ntfy.server} onInput={(v) => set(['ntfy', 'server'], v)} />
            </Field>
            <Field label={t('settings.alerts.topic')} error={err('ntfy.topic')} hint={t('settings.alerts.ntfyTopicsPublic')}>
              <TextInput dataField="alerts.ntfy.topic" value={alerts.ntfy.topic} onInput={(v) => set(['ntfy', 'topic'], v)} />
            </Field>
            <Field label={t('settings.alerts.token')}>
              <TextInput
                type="password"
                value={alerts.ntfy.token}
                placeholder={t('settings.alerts.optional')}
                onInput={(v) => set(['ntfy', 'token'], v)}
              />
            </Field>
          </div>
          <TestButton view={view} channel="ntfy" />
        </>
      )}
    </Group>
  );
};

const WebhookGroup: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, set, err, dep, fix } = view;
  return (
    <Group>
      <DepToggle
        entry={dep('alerts.webhook.enabled')}
        onFix={fix}
        label={t('settings.alerts.webhook')}
        checked={alerts.webhook.enabled}
        onChange={(v) => set(['webhook', 'enabled'], v)}
        hint={t('settings.alerts.postsJsonDeviceEventTitle')}
      />
      {alerts.webhook.enabled && (
        <>
          <div class="form-grid">
            <Field label="URL" error={err('webhook.url')}>
              <TextInput
                dataField="alerts.webhook.url"
                type="url"
                value={alerts.webhook.url}
                placeholder="http://homeassistant.local:8123/api/webhook/..."
                onInput={(v) => set(['webhook', 'url'], v)}
              />
            </Field>
            <Field label={t('settings.alerts.authorizationHeader')}>
              <TextInput
                type="password"
                value={alerts.webhook.authHeader}
                placeholder={t('settings.alerts.optional')}
                onInput={(v) => set(['webhook', 'authHeader'], v)}
              />
            </Field>
          </div>
          {alerts.webhook.url.startsWith('https://') && (
            <Toggle
              label={t('settings.alerts.skipCertificateChecks')}
              checked={alerts.webhook.insecureTls}
              onChange={(v) => set(['webhook', 'insecureTls'], v)}
              hint={t('settings.alerts.onlyForASelfSigned')}
            />
          )}
          <TestButton view={view} channel="webhook" />
        </>
      )}
    </Group>
  );
};

// Where alerts go: Pushover, ntfy, a webhook and MQTT.
export const AlertChannelsCard: FunctionalComponent<{ view: AlertsView }> = ({ view }) => (
  <SettingsCard title={t('settings.alerts.channels')} hint={t('settings.alerts.testsUseTheSavedSettings')}>
    <PushoverGroup view={view} />
    <NtfyGroup view={view} />
    <WebhookGroup view={view} />
    <Group>
      <DepToggle
        entry={view.dep('alerts.mqtt.enabled')}
        onFix={view.fix}
        label="MQTT"
        checked={view.alerts.mqtt.enabled}
        onChange={(v) => view.set(['mqtt', 'enabled'], v)}
        hint={t('settings.alerts.publishesEachAlertToTopic', { topic: view.config.mqtt.topic })}
      />
      <TestButton view={view} channel="mqtt" />
    </Group>
  </SettingsCard>
);
