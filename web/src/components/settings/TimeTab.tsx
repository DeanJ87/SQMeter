import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { Field, Group, NumberInput, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

// Common timezones in POSIX TZ format
export const TIMEZONE_OPTIONS = [
  { label: 'UTC', value: 'UTC0' },
  { label: 'GMT (no daylight saving)', value: 'GMT0' },
  { label: 'US/Pacific (PST)', value: 'PST8PDT,M3.2.0,M11.1.0' },
  { label: 'US/Mountain (MST)', value: 'MST7MDT,M3.2.0,M11.1.0' },
  { label: 'US/Central (CST)', value: 'CST6CDT,M3.2.0,M11.1.0' },
  { label: 'US/Eastern (EST)', value: 'EST5EDT,M3.2.0,M11.1.0' },
  { label: 'Europe/London (GMT)', value: 'GMT0BST,M3.5.0/1,M10.5.0' },
  { label: 'Europe/Paris (CET)', value: 'CET-1CEST,M3.5.0,M10.5.0/3' },
  { label: 'Australia/Sydney (AEST)', value: 'AEST-10AEDT,M10.1.0,M4.1.0/3' },
  { label: 'Asia/Tokyo (JST)', value: 'JST-9' },
];

const NTP = 0;
const GPS = 1;
const SOURCE_LABEL: Record<number, string> = { [NTP]: 'NTP (internet)', [GPS]: 'GPS' };

const TimeTab: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, hw }) => {
  const knownZone = TIMEZONE_OPTIONS.some((tz) => tz.value === config.ntp.timezone);
  const lastSourceReason = 'At least one time source has to stay on.';
  const bothSources = config.ntp.enabled && config.gps.enabled;

  const setPrimary = (primary: number) => {
    const other = primary === NTP ? GPS : NTP;
    updateMany([
      [['primaryTimeSource'], primary],
      [['secondaryTimeSource'], other],
    ]);
  };

  const gpsBadge = !config.gps.enabled
    ? <StatusBadge tone="off" label="Off" />
    : hw.gps.detected === null
      ? undefined
      : <StatusBadge tone={hw.gps.detected ? 'ok' : 'bad'} label={hw.gps.detected ? 'Running' : 'Not detected'} />;

  return (
    <>
      <SettingsCard title="Time zone">
        <Field label="Time zone" hint="Used for timestamps, logs and the RG-15 daily reset.">
          <SelectInput
            dataField="ntp.timezone"
            value={knownZone ? config.ntp.timezone : 'custom'}
            options={[...TIMEZONE_OPTIONS, { value: 'custom', label: 'Custom (POSIX TZ string)...' }]}
            onChange={(v) => update(['ntp', 'timezone'], v === 'custom' ? '' : v)}
          />
        </Field>
        {!knownZone && (
          <Field label="Custom time zone" hint="POSIX format, e.g. PST8PDT,M3.2.0,M11.1.0" error={error('ntp.timezone')}>
            <TextInput dataField="ntp.timezone" value={config.ntp.timezone} onInput={(v) => update(['ntp', 'timezone'], v)} />
          </Field>
        )}
      </SettingsCard>

      <SettingsCard title="Time sources" description="Accurate time matters for logs, alerts and Alpaca timestamps.">
        <Group title="NTP" aside={<StatusBadge tone={config.ntp.enabled ? 'ok' : 'off'} label={config.ntp.enabled ? 'On' : 'Off'} />}>
          <Toggle
            label="Sync time from the internet (NTP)"
            checked={config.ntp.enabled}
            onChange={(v) => update(['ntp', 'enabled'], v)}
            disabled={config.ntp.enabled && !config.gps.enabled}
            hint={config.ntp.enabled && !config.gps.enabled ? lastSourceReason : undefined}
          />
          {error('ntp.enabled') && <p class="text-xs text-red-400">{error('ntp.enabled')}</p>}
          {config.ntp.enabled && (
            <div class="grid grid-cols-1 md:grid-cols-3 gap-4 ml-7">
              <Field label="Primary server" error={error('ntp.server1')}>
                <TextInput dataField="ntp.server1" value={config.ntp.server1} placeholder="pool.ntp.org" onInput={(v) => update(['ntp', 'server1'], v)} />
              </Field>
              <Field label="Secondary server">
                <TextInput dataField="ntp.server2" value={config.ntp.server2} placeholder="time.nist.gov" onInput={(v) => update(['ntp', 'server2'], v)} />
              </Field>
              <Field label="Sync every (minutes)" error={error('ntp.syncIntervalMs')}>
                <NumberInput
                  dataField="ntp.syncIntervalMs"
                  integer
                  min={10}
                  max={1440}
                  value={config.ntp.syncIntervalMs / 60000}
                  onChange={(v) => update(['ntp', 'syncIntervalMs'], v * 60000)}
                />
              </Field>
            </div>
          )}
        </Group>

        <Group title="GPS" aside={gpsBadge}>
          <Toggle
            dataField="gps.enabled"
            label="Use a GPS receiver (time and location)"
            checked={config.gps.enabled}
            onChange={(v) => update(['gps', 'enabled'], v)}
            disabled={config.gps.enabled && !config.ntp.enabled}
            hint={config.gps.enabled && !config.ntp.enabled ? lastSourceReason : 'Works without internet. Needs a sky view for a fix.'}
          />
          {config.gps.enabled && (
            <div class="grid grid-cols-1 md:grid-cols-3 gap-4 ml-7">
              <Field label="RX pin" error={error('gps.rxPin')}>
                <NumberInput dataField="gps.rxPin" integer min={0} max={39} value={config.gps.rxPin} onChange={(v) => update(['gps', 'rxPin'], v)} />
              </Field>
              <Field label="TX pin" error={error('gps.txPin')}>
                <NumberInput dataField="gps.txPin" integer min={0} max={39} value={config.gps.txPin} onChange={(v) => update(['gps', 'txPin'], v)} />
              </Field>
              <Field label="Baud rate" error={error('gps.baudRate')}>
                <SelectInput
                  dataField="gps.baudRate"
                  value={String(config.gps.baudRate)}
                  options={['4800', '9600', '19200', '38400', '57600', '115200'].map((b) => ({ value: b, label: b }))}
                  onChange={(v) => update(['gps', 'baudRate'], parseInt(v, 10))}
                />
              </Field>
            </div>
          )}
        </Group>

        {bothSources && (
          <Group title="Priority">
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <Field label="Use first" error={error('primaryTimeSource')} hint={`Falls back to ${SOURCE_LABEL[config.secondaryTimeSource]} if unavailable.`}>
                <SelectInput
                  dataField="primaryTimeSource"
                  value={String(config.primaryTimeSource)}
                  options={[
                    { value: String(NTP), label: SOURCE_LABEL[NTP] },
                    { value: String(GPS), label: SOURCE_LABEL[GPS] },
                  ]}
                  onChange={(v) => setPrimary(parseInt(v, 10))}
                />
              </Field>
            </div>
          </Group>
        )}
      </SettingsCard>
    </>
  );
};

export default TimeTab;
