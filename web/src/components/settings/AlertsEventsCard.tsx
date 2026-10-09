import { ComponentChildren, FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { AlertEventKey, SystemStatus } from '../../types';
import type { DepEntry } from '../../lib/settingsDeps';
import { deviceTime } from '../../lib/deviceTime';
import { describeDarkness, type DarknessView } from './darkness';
import { minutesToSeconds, secondsToMinutes } from './alertSchedule';
import { LEVEL_HINT, type AlertsView } from './alertsShared';
import { AlertEventRow } from './AlertEventRow';
import { DepNote, DepToggle, Field, NumberInput, SelectInput, SettingsCard } from './controls';
import { InfoTip, Note, ReadingRow } from '../ui';
import { t } from '../../i18n';

// A GPS fix wins over the location in Settings (saved or not).
const darknessLocation = (view: AlertsView, status: SystemStatus | null) => {
  const sky = status?.sky;
  if (sky?.locationSource === 'gps' && sky.latitude !== undefined && sky.longitude !== undefined) {
    return { latitude: sky.latitude, longitude: sky.longitude };
  }
  return view.config.location?.set ? view.config.location : null;
};

const EventRows: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, off, set, err } = view;
  const [editing, setEditing] = useState<AlertEventKey | null>(null);
  const row = (key: AlertEventKey, label: string, opts: { hint?: string; threshold?: ComponentChildren } = {}) => (
    <AlertEventRow
      view={view}
      eventKey={key}
      label={label}
      hint={opts.hint}
      threshold={opts.threshold}
      editing={editing === key}
      onEdit={() => setEditing(editing === key ? null : key)}
    />
  );
  return (
    <>
      {row('unsafe', t('settings.alerts.itTurnsUnsafe'), { hint: t('settings.alerts.listsTheReasons') })}
      {row('safe', t('settings.alerts.itSSafeAgain'))}
      {row('rain_started', t('settings.alerts.rainStarts'))}
      {row('rain_stopped', t('settings.alerts.rainStops'))}
      {row('sensor_fault', t('settings.alerts.aSensorFails'), { hint: t('settings.alerts.includesTheRg15Lens') })}
      {row('sensor_recovered', t('settings.alerts.aSensorRecovers'))}
      {row('dew_risk', t('settings.alerts.dewRiskWithin'), {
        threshold: (
          <NumberInput
            min={0}
            max={10}
            step={0.5}
            unit="°C"
            ariaLabel={t('settings.alerts.dewRiskMargin')}
            value={alerts.dewRiskMarginC}
            disabled={off}
            onChange={(v) => set(['dewRiskMarginC'], v)}
          />
        ),
        hint: t('settings.alerts.temperatureWithinThisMarginOf'),
      })}
      {row('clear_sky', t('settings.alerts.skiesClearUpBelow'), {
        threshold: (
          <NumberInput
            min={0}
            max={100}
            step={1}
            unit="%"
            ariaLabel={t('settings.alerts.clearBelow')}
            value={alerts.clearSkyCloudPercent}
            disabled={off}
            onChange={(v) => set(['clearSkyCloudPercent'], v)}
          />
        ),
        hint: t('settings.alerts.cloudCover2'),
      })}
      {row('clouded_over', t('settings.alerts.skiesCloudOverAbove'), {
        threshold: (
          <NumberInput
            min={0}
            max={100}
            step={1}
            unit="%"
            ariaLabel={t('settings.alerts.cloudedOverAbove')}
            value={alerts.cloudedOverCloudPercent}
            error={err('cloudedOverCloudPercent')}
            disabled={off}
            onChange={(v) => set(['cloudedOverCloudPercent'], v)}
          />
        ),
        hint: t('settings.alerts.cloudCover2'),
      })}
      {row('client_lost', t('settings.alerts.theImagingAppStopsChecking'), { hint: t('settings.alerts.noRequestReachedTheSafety') })}
      {row('client_back', t('settings.alerts.theImagingAppIsBack'), { hint: t('settings.alerts.itStartedCheckingAgainAfter') })}
      {row('client_disconnected', t('settings.alerts.theImagingAppDisconnects'), {
        hint: t('settings.alerts.itDisconnectedNormallyForExample'),
      })}
    </>
  );
};

const SilentForFields: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, off, set, err } = view;
  return (
    <div class="form-grid">
      <Field
        label={t('settings.alerts.silentForSafetyMonitor')}
        error={err('clientSilentSafetySeconds')}
        hint={t('settings.alerts.howLongWithoutARequest')}
      >
        <NumberInput
          min={0.5}
          max={60}
          step={0.5}
          unit="min"
          ariaLabel={t('settings.alerts.silentForSafetyMonitor')}
          value={secondsToMinutes(alerts.clientSilentSafetySeconds ?? 120)}
          disabled={off}
          onChange={(v) => set(['clientSilentSafetySeconds'], minutesToSeconds(v))}
        />
      </Field>
      <Field
        label={t('settings.alerts.silentForWeatherDevice')}
        error={err('clientSilentWeatherSeconds')}
        hint={t('settings.alerts.imagingAppsCheckWeatherLess')}
      >
        <NumberInput
          min={0.5}
          max={60}
          step={0.5}
          unit="min"
          ariaLabel={t('settings.alerts.silentForWeatherDevice')}
          value={secondsToMinutes(alerts.clientSilentWeatherSeconds ?? 600)}
          disabled={off}
          onChange={(v) => set(['clientSilentWeatherSeconds'], minutesToSeconds(v))}
        />
      </Field>
    </div>
  );
};

const NightOnlyRow: FunctionalComponent<{ view: AlertsView }> = ({ view }) => {
  const { alerts, off, set, dep, fix } = view;
  return (
    <div class="rule-row">
      <div class="toggle-stack">
        <DepToggle
          entry={dep('alerts.skyNightOnly')}
          onFix={fix}
          label={t('settings.alerts.skyAlertsOnlyWhenIt')}
          checked={alerts.skyNightOnly}
          onChange={(v) => set(['skyNightOnly'], v)}
          disabled={off}
          hint={t('settings.alerts.fromTheSunSPosition')}
        />
        <DepToggle
          entry={dep('alerts.safetyNightOnly')}
          onFix={fix}
          label={t('settings.alerts.safetyAlertsOnlyWhenIt')}
          checked={alerts.safetyNightOnly}
          onChange={(v) => set(['safetyNightOnly'], v)}
          disabled={off}
          hint={t('settings.alerts.soDawnBrighteningTheSky')}
        />
      </div>
      <SelectInput
        value={String(alerts.nightSunAltitudeDeg)}
        ariaLabel={t('settings.alerts.darkMeans')}
        disabled={off || (!alerts.skyNightOnly && !alerts.safetyNightOnly)}
        options={[
          { value: '-0.833', label: t('settings.alerts.afterSunset') },
          { value: '-12', label: t('settings.alerts.nauticalDark12') },
          { value: '-18', label: t('settings.alerts.astronomicalDark18') },
        ]}
        onChange={(v) => set(['nightSunAltitudeDeg'], Number(v))}
      />
    </div>
  );
};

// The sun now and the dark period, one row each (spec 026 A10).
const DarknessRows: FunctionalComponent<{ view: DarknessView }> = ({ view }) => (
  <div class="darkness-rows">
    {view.sun && <ReadingRow label={t('settings.alerts.sunNow')} value={view.sun} />}
    <div class="reading-row">
      <span class="reading-label">
        {t('settings.alerts.darkness')} <InfoTip text={view.hint} />
      </span>
      <strong class="reading-value">{view.dark}</strong>
    </div>
    {view.unsaved && <Note tone="warn">{t('settings.alerts.saveToApplyLimit')}</Note>}
  </div>
);

// "Notify me when": one row per event, then the timing rules.
export const AlertsEventsCard: FunctionalComponent<{ view: AlertsView; status: SystemStatus | null; wakePhones: DepEntry }> = ({
  view,
  status,
  wakePhones,
}) => {
  const { alerts, off, set, err, fix, pushoverOn } = view;
  const darkness = describeDarkness({
    sky: status?.sky,
    location: darknessLocation(view, status),
    formLimitDeg: alerts.nightSunAltitudeDeg,
    deviceNow: deviceTime(status),
  });
  return (
    <SettingsCard title={t('settings.alerts.notifyMeWhen')} hint={LEVEL_HINT}>
      <fieldset class="card-body" disabled={off}>
        <div class={`event-table${pushoverOn ? ' with-sound' : ''}`}>
          <div class="event-row event-table-head" aria-hidden="true">
            <span />
            <span>{t('settings.alerts.level')}</span>
            {pushoverOn && <span>{t('settings.alerts.pushoverSound')}</span>}
            <span />
            <span />
          </div>
          <EventRows view={view} />
        </div>
        {err('cloudedOverCloudPercent') && <Note tone="bad">{err('cloudedOverCloudPercent')}</Note>}
        {wakePhones.state === 'inactive' && <DepNote entry={wakePhones} onFix={fix} prefix={t('settings.alerts.phonesWontRing')} />}
        <SilentForFields view={view} />
        <NightOnlyRow view={view} />
        {darkness && <DarknessRows view={darkness} />}
        <div class="form-grid">
          <Field label={t('settings.alerts.cooldown')} error={err('cooldownSeconds')} hint={t('settings.alerts.minimumGapBetweenAlertsOf')}>
            <NumberInput
              integer
              min={0}
              max={86400}
              unit="s"
              value={alerts.cooldownSeconds}
              disabled={off}
              onChange={(v) => set(['cooldownSeconds'], v)}
            />
          </Field>
        </div>
      </fieldset>
    </SettingsCard>
  );
};
