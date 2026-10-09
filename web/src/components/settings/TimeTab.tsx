import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { Note } from '../ui';
import { defaultLocationConfig } from './defaults';
import type { SettingsTabProps } from './context';
import { ActionButton, DepToggle, Field, Group, NumberInput, SelectInput, SettingsCard, StatusBadge, TextInput } from './controls';
import { t } from '../../i18n';
import { formatCoordinatesInput } from '../../i18n/format';
import { parseCoordinates } from '../../i18n/parse';

// Common time zones in POSIX TZ format
export const TIMEZONE_OPTIONS = [
  { label: 'UTC', value: 'UTC0' },
  { label: t('settings.time.gmtNoDaylightSaving'), value: 'GMT0' },
  { label: t('settings.time.usPacificPst'), value: 'PST8PDT,M3.2.0,M11.1.0' },
  { label: t('settings.time.usMountainMst'), value: 'MST7MDT,M3.2.0,M11.1.0' },
  { label: t('settings.time.usCentralCst'), value: 'CST6CDT,M3.2.0,M11.1.0' },
  { label: t('settings.time.usEasternEst'), value: 'EST5EDT,M3.2.0,M11.1.0' },
  { label: t('settings.time.europeLondonGmt'), value: 'GMT0BST,M3.5.0/1,M10.5.0' },
  { label: t('settings.time.europeParisCet'), value: 'CET-1CEST,M3.5.0,M10.5.0/3' },
  { label: t('settings.time.australiaSydneyAest'), value: 'AEST-10AEDT,M10.1.0,M4.1.0/3' },
  { label: t('settings.time.asiaTokyoJst'), value: 'JST-9' },
];

// "51.4779, -0.0015", "51,4779; -0,0015", "51,4779 -0,0015"... -> [lat, lon]; null if it isn't that.
export { parseCoordinates };

const NTP = 0;
const GPS = 1;
const SOURCE_LABEL: Record<number, string> = { [NTP]: 'NTP', [GPS]: 'GPS' };
const LAST_SOURCE = t('settings.time.atLeastOneTimeSource');

const knownZone = (timezone: string) => TIMEZONE_OPTIONS.some((tz) => tz.value === timezone);

const TimeZoneCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error }) => (
  <SettingsCard title={t('settings.time.timeZone')}>
    <div class="form-grid">
      <Field label={t('settings.time.timeZone')}>
        <SelectInput
          dataField="ntp.timezone"
          value={knownZone(config.ntp.timezone) ? config.ntp.timezone : 'custom'}
          options={[...TIMEZONE_OPTIONS, { value: 'custom', label: t('common.custom') }]}
          onChange={(v) => update(['ntp', 'timezone'], v === 'custom' ? '' : v)}
        />
      </Field>
      {!knownZone(config.ntp.timezone) && (
        <Field label={t('settings.time.posixTimeZone')} hint={t('settings.time.eGPst8pdtM32')} error={error('ntp.timezone')}>
          <TextInput dataField="ntp.timezone" value={config.ntp.timezone} onInput={(v) => update(['ntp', 'timezone'], v)} />
        </Field>
      )}
    </div>
  </SettingsCard>
);

// The sun's altitude now, once the device knows where it is.
const SunNote: FunctionalComponent<{ sky: NonNullable<SettingsTabProps['status']>['sky'] }> = ({ sky }) =>
  sky?.nightKnown && sky.sunAltitudeDeg !== undefined ? (
    <Note>
      {sky.isNight
        ? t('settings.time.sunAtDarkNow', { altitude: sky.sunAltitudeDeg })
        : t('settings.time.sunAtNotDarkYet', { altitude: sky.sunAltitudeDeg })}
    </Note>
  ) : null;

// The observatory's coordinates (typed, or from the browser) and the Sun & Moon card.
const LocationCard: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, status, deps, fix }) => {
  const location = { ...defaultLocationConfig, ...config.location };
  const [coords, setCoords] = useState(location.set ? formatCoordinatesInput(location.latitude, location.longitude) : '');
  const [coordsError, setCoordsError] = useState<string | null>(null);
  const [locating, setLocating] = useState(false);
  const canUseBrowserLocation = typeof window !== 'undefined' && window.isSecureContext && 'geolocation' in navigator;
  const sky = status?.sky;

  const applyCoords = (text: string) => {
    setCoords(text);
    if (text.trim() === '') {
      setCoordsError(null);
      update(['location', 'set'], false);
      return;
    }
    const parsed = parseCoordinates(text);
    setCoordsError(parsed ? null : t('settings.time.enterLatitudeLongitudeEG'));
    if (parsed) {
      updateMany([
        [['location', 'set'], true],
        [['location', 'latitude'], Math.round(parsed[0] * 1e4) / 1e4],
        [['location', 'longitude'], Math.round(parsed[1] * 1e4) / 1e4],
      ]);
    }
  };

  const useBrowserLocation = () => {
    setLocating(true);
    navigator.geolocation.getCurrentPosition(
      (position) => {
        setLocating(false);
        applyCoords(formatCoordinatesInput(position.coords.latitude, position.coords.longitude));
      },
      () => {
        setLocating(false);
        setCoordsError(t('settings.time.theBrowserDidnTShare'));
      },
      { timeout: 15000 },
    );
  };

  return (
    <SettingsCard
      id="location"
      title={t('settings.time.location')}
      hint={t('settings.time.usedToWorkOutWhen')}
      badge={sky?.locationSource === 'gps' ? <StatusBadge tone="ok" label={t('settings.time.usingGps')} /> : undefined}
    >
      <Field
        class="field-wide"
        label={t('settings.time.coordinates')}
        error={coordsError ?? error('location.latitude') ?? error('location.longitude')}
        hint={t('settings.time.latitudeLongitudeInDecimalDegrees')}
      >
        <div class="input-row">
          <TextInput dataField="location.latitude" value={coords} placeholder="51.4779, -0.0015" onInput={applyCoords} />
          {canUseBrowserLocation && (
            <ActionButton onClick={useBrowserLocation} busy={locating} busyLabel={t('common.locating')}>
              {t('settings.time.useMyLocation')}
            </ActionButton>
          )}
        </div>
      </Field>
      <SunNote sky={sky} />
      <DepToggle
        entry={deps.get('location.showSunMoon')}
        onFix={fix}
        label={t('settings.time.sunMoonCardOnThe')}
        checked={location.showSunMoon !== false}
        onChange={(v) => update(['location', 'showSunMoon'], v)}
        hint={t('settings.time.twilightDarknessMoonPhaseAnd')}
      />
    </SettingsCard>
  );
};

const NtpGroup: FunctionalComponent<SettingsTabProps> = ({ config, update, error, deps, fix }) => (
  <Group title="NTP">
    <DepToggle
      entry={deps.get('ntp.enabled')}
      onFix={fix}
      label={t('settings.time.internetTimeNtp')}
      checked={config.ntp.enabled}
      onChange={(v) => update(['ntp', 'enabled'], v)}
      disabled={config.ntp.enabled && !config.gps.enabled}
      hint={config.ntp.enabled && !config.gps.enabled ? LAST_SOURCE : undefined}
    />
    {config.ntp.enabled && (
      <div class="form-grid">
        <Field label={t('settings.time.server')} error={error('ntp.server1')}>
          <TextInput
            dataField="ntp.server1"
            value={config.ntp.server1}
            placeholder="pool.ntp.org"
            onInput={(v) => update(['ntp', 'server1'], v)}
          />
        </Field>
        <Field label={t('settings.time.fallbackServer')}>
          <TextInput
            dataField="ntp.server2"
            value={config.ntp.server2}
            placeholder="time.nist.gov"
            onInput={(v) => update(['ntp', 'server2'], v)}
          />
        </Field>
        <Field label={t('settings.time.syncEvery')} error={error('ntp.syncIntervalMs')}>
          <NumberInput
            dataField="ntp.syncIntervalMs"
            integer
            min={10}
            max={1440}
            unit="min"
            value={config.ntp.syncIntervalMs / 60000}
            onChange={(v) => update(['ntp', 'syncIntervalMs'], v * 60000)}
          />
        </Field>
      </div>
    )}
  </Group>
);

const GpsGroup: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, deps, fix }) => {
  // GPS starts at boot; waiting for a restart is shown under the switch (D-35).
  const gpsBadge = config.gps.enabled && hw.gps.detected ? <StatusBadge tone="ok" label={t('settings.time.running')} /> : undefined;
  return (
    <Group title="GPS" aside={gpsBadge}>
      <DepToggle
        entry={deps.get('gps.enabled')}
        onFix={fix}
        dataField="gps.enabled"
        label={t('settings.time.gpsReceiver')}
        checked={config.gps.enabled}
        onChange={(v) => update(['gps', 'enabled'], v)}
        disabled={config.gps.enabled && !config.ntp.enabled}
        hint={config.gps.enabled && !config.ntp.enabled ? LAST_SOURCE : t('settings.time.timeAndLocationWithoutInternet')}
      />
      {config.gps.enabled && (
        <div class="form-grid">
          <Field label={t('settings.time.rxPin')} error={error('gps.rxPin')}>
            <NumberInput
              dataField="gps.rxPin"
              integer
              min={0}
              max={39}
              value={config.gps.rxPin}
              onChange={(v) => update(['gps', 'rxPin'], v)}
            />
          </Field>
          <Field label={t('settings.time.txPin')} error={error('gps.txPin')}>
            <NumberInput
              dataField="gps.txPin"
              integer
              min={0}
              max={39}
              value={config.gps.txPin}
              onChange={(v) => update(['gps', 'txPin'], v)}
            />
          </Field>
          <Field label={t('settings.time.baudRate')} error={error('gps.baudRate')}>
            <SelectInput
              dataField="gps.baudRate"
              value={String(config.gps.baudRate)}
              options={['4800', '9600', '19200', '38400', '57600', '115200'].map((b) => ({ value: b, label: b }))}
              onChange={(v) => update(['gps', 'baudRate'], Number(v))}
            />
          </Field>
        </div>
      )}
    </Group>
  );
};

// Which source is tried first when both are on.
const PriorityGroup: FunctionalComponent<SettingsTabProps> = ({ config, updateMany, error }) => (
  <Group title={t('settings.time.priority')}>
    <div class="form-grid">
      <Field
        label={t('settings.time.useFirst')}
        error={error('primaryTimeSource')}
        hint={t('settings.time.fallsBackToValue', { value: SOURCE_LABEL[config.secondaryTimeSource] })}
      >
        <SelectInput
          dataField="primaryTimeSource"
          value={String(config.primaryTimeSource)}
          options={[
            { value: String(NTP), label: 'NTP' },
            { value: String(GPS), label: 'GPS' },
          ]}
          onChange={(v) => {
            const primary = Number(v);
            updateMany([
              [['primaryTimeSource'], primary],
              [['secondaryTimeSource'], primary === NTP ? GPS : NTP],
            ]);
          }}
        />
      </Field>
    </div>
  </Group>
);

const TimeTab: FunctionalComponent<SettingsTabProps> = (props) => {
  const { config } = props;
  const bothSources = config.ntp.enabled && config.gps.enabled;
  return (
    <>
      <TimeZoneCard {...props} />

      <LocationCard {...props} />

      <SettingsCard id="time-sources" title={t('settings.time.timeSources')}>
        <NtpGroup {...props} />

        <GpsGroup {...props} />

        {bothSources && <PriorityGroup {...props} />}
      </SettingsCard>
    </>
  );
};

export default TimeTab;
