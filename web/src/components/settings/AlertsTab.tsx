import { FunctionalComponent } from 'preact';
import { useRef } from 'preact/hooks';
import type { AlertSendMode, SystemStatus } from '../../types';
import { mergeAlertsConfig } from './defaults';
import { showToast } from '../toast';
import type { SettingsTabProps } from './context';
import { useAlertSchedule } from '../../hooks/useAlertSchedule';
import { useAlertTest } from '../../hooks/useAlertTest';
import { PAUSE_HINT, SEND_MODE_OPTIONS, describeSchedule, describeSilentClients } from './alertSchedule';
import { withoutAlertsOff, type AlertsView } from './alertsShared';
import { AlertsEventsCard } from './AlertsEventsCard';
import { AlertChannelsCard } from './AlertChannelsCard';
import { InfoTip, Note } from '../ui';
import { ActionButton, DepNote, Group, Requires, SelectInput, SettingsCard, StatusBadge, Toggle } from './controls';
import { t } from '../../i18n';

// Paused or sending, and Pause/Resume: live device state, not a saved setting.
type Schedule = ReturnType<typeof useAlertSchedule>;

const ScheduleStatus: FunctionalComponent<{ schedule: Schedule; sendMode: AlertSendMode }> = ({ schedule, sendMode }) => {
  const { schedule: shownSchedule, busy: scheduleBusy, pauseOrResume: sendPauseOrResume } = schedule;
  const pauseOrResume = async (resume: boolean) => {
    if (!(await sendPauseOrResume(resume))) showToast({ message: t('settings.alerts.couldNotReachTheDevice'), tone: 'bad' });
  };
  return (
    <div class="btn-row" data-schedule-status>
      {shownSchedule && (
        <Note tone={shownSchedule.armed ? 'ok' : 'warn'}>
          {describeSchedule({ ...shownSchedule, mode: shownSchedule.mode ?? sendMode })}
        </Note>
      )}
      <ActionButton onClick={() => pauseOrResume(!(shownSchedule?.armed ?? true))} disabled={shownSchedule === null || scheduleBusy}>
        {shownSchedule?.armed === false ? t('settings.alerts.resumeAlerts') : t('settings.alerts.pauseAlerts')}
      </ActionButton>
      <InfoTip text={PAUSE_HINT} />
    </div>
  );
};

// When alerts are sent.
const WhenToSend: FunctionalComponent<{ view: AlertsView; status: SystemStatus | null; schedule: Schedule }> = ({
  view,
  status,
  schedule,
}) => {
  const { alerts, updateMany, dep, fix } = view;
  const sendMode: AlertSendMode = alerts.sendMode ?? (alerts.armWithAlpaca ? 'whileConnected' : 'any');
  const silentClients = status?.alpaca?.clients && describeSilentClients(status.alpaca.clients);
  return (
    <Group title={t('settings.alerts.whenToSend')}>
      <SelectInput
        value={sendMode}
        ariaLabel={t('settings.alerts.whenToSend')}
        options={SEND_MODE_OPTIONS.map(({ value, label }) => ({ value, label }))}
        onChange={(v) =>
          updateMany([
            [['alerts', 'sendMode'], v],
            [['alerts', 'armWithAlpaca'], v === 'whileConnected'],
          ])
        }
      />
      <Note>{SEND_MODE_OPTIONS.find((option) => option.value === sendMode)?.help}</Note>
      {dep('alerts.sendMode').state === 'inactive' && <DepNote entry={dep('alerts.sendMode')} onFix={fix} />}
      <ScheduleStatus schedule={schedule} sendMode={sendMode} />
      {silentClients && <Note tone="warn">{silentClients}</Note>}
    </Group>
  );
};

// The Alerts card: the master switch, channel warnings and when to send.
const SendAlertsCard: FunctionalComponent<{ view: AlertsView; status: SystemStatus | null; schedule: Schedule; channelsOn: number }> = ({
  view,
  status,
  schedule,
  channelsOn,
}) => {
  const { alerts, off, set, channelCount } = view;
  const noChannels = channelCount === 0;
  return (
    <SettingsCard
      id="alerts"
      title={t('settings.alerts.alerts')}
      hint={t('settings.alerts.pushNotificationsSentByThe')}
      badge={
        off ? undefined : (
          <StatusBadge
            tone={noChannels ? 'warn' : 'ok'}
            label={noChannels ? t('settings.alerts.noChannels') : t('settings.alerts.channelCount', { count: channelCount })}
          />
        )
      }
    >
      <Toggle label={t('settings.alerts.sendAlerts')} checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
      {!off && channelsOn === 0 && <Requires tone="warn">{t('settings.alerts.turnOnAChannelBelow')}</Requires>}
      {!off && channelsOn > 0 && noChannels && <Requires tone="warn">{t('settings.alerts.alertsReachNowhereNoChannel')}</Requires>}
      {!off && <WhenToSend view={view} status={status} schedule={schedule} />}
    </SettingsCard>
  );
};

const AlertsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, status, dirty, deps, fix }) => {
  const dep = (setting: string) => withoutAlertsOff(deps.get(setting));
  const alerts = mergeAlertsConfig(config.alerts);
  const tester = useAlertTest();
  const schedule = useAlertSchedule(status?.alerts);
  const lastField = useRef<AlertsView['lastField']['current']>(null);
  // Counts only channels that can deliver (FR-007).
  const channelEntries = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).map((channel) => dep(`alerts.${channel}.enabled`));
  const channelsOn = channelEntries.filter((e) => e.state !== 'off').length;
  const view: AlertsView = {
    config,
    alerts,
    off: !alerts.enabled,
    dirty,
    pushoverOn: alerts.pushover.enabled,
    channelCount: channelEntries.filter((e) => e.state === 'active' || e.state === 'unknown').length,
    set: (path, value) => update(['alerts', ...path], value),
    updateMany,
    err: (key) => error(`alerts.${key}`),
    dep,
    fix,
    tester,
    lastField,
  };

  return (
    <>
      <SendAlertsCard view={view} status={status} schedule={schedule} channelsOn={channelsOn} />
      <AlertsEventsCard view={view} status={status} wakePhones={deps.get('alerts.wakePhones')} />
      <AlertChannelsCard view={view} />
    </>
  );
};

export default AlertsTab;
