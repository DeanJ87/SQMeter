import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { Field, Group, NumberInput, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

// Common time zones in POSIX TZ format
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
const SOURCE_LABEL: Record<number, string> = { [NTP]: 'NTP', [GPS]: 'GPS' };
const LAST_SOURCE = 'At least one time source has to stay on.';

const TimeTab: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, hw }) => {
  const knownZone = TIMEZONE_OPTIONS.some((tz) => tz.value === config.ntp.timezone);
  const bothSources = config.ntp.enabled && config.gps.enabled;

  const gpsBadge = !config.gps.enabled
    ? undefined
    : hw.gps.detected === null
      ? undefined
      : <StatusBadge tone={hw.gps.detected ? 'ok' : 'bad'} label={hw.gps.detected ? 'Running' : 'Not detected'} />;

  return (
    <>
      <SettingsCard title="Time zone">
        <div class="form-grid">
          <Field label="Time zone">
            <SelectInput
              dataField="ntp.timezone"
              value={knownZone ? config.ntp.timezone : 'custom'}
              options={[...TIMEZONE_OPTIONS, { value: 'custom', label: 'Custom...' }]}
              onChange={(v) => update(['ntp', 'timezone'], v === 'custom' ? '' : v)}
            />
          </Field>
          {!knownZone && (
            <Field label="POSIX time zone" hint="e.g. PST8PDT,M3.2.0,M11.1.0" error={error('ntp.timezone')}>
              <TextInput dataField="ntp.timezone" value={config.ntp.timezone} onInput={(v) => update(['ntp', 'timezone'], v)} />
            </Field>
          )}
        </div>
      </SettingsCard>

      <SettingsCard title="Time sources">
        <Group title="NTP">
          <Toggle
            label="Internet time (NTP)"
            checked={config.ntp.enabled}
            onChange={(v) => update(['ntp', 'enabled'], v)}
            disabled={config.ntp.enabled && !config.gps.enabled}
            hint={config.ntp.enabled && !config.gps.enabled ? LAST_SOURCE : undefined}
          />
          {config.ntp.enabled && (
            <div class="form-grid">
              <Field label="Server" error={error('ntp.server1')}>
                <TextInput dataField="ntp.server1" value={config.ntp.server1} placeholder="pool.ntp.org" onInput={(v) => update(['ntp', 'server1'], v)} />
              </Field>
              <Field label="Fallback server">
                <TextInput dataField="ntp.server2" value={config.ntp.server2} placeholder="time.nist.gov" onInput={(v) => update(['ntp', 'server2'], v)} />
              </Field>
              <Field label="Sync every" error={error('ntp.syncIntervalMs')}>
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

        <Group title="GPS" aside={gpsBadge}>
          <Toggle
            dataField="gps.enabled"
            label="GPS receiver"
            checked={config.gps.enabled}
            onChange={(v) => update(['gps', 'enabled'], v)}
            disabled={config.gps.enabled && !config.ntp.enabled}
            hint={config.gps.enabled && !config.ntp.enabled ? LAST_SOURCE : 'Time and location without internet.'}
          />
          {config.gps.enabled && (
            <div class="form-grid">
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
            <div class="form-grid">
              <Field label="Use first" error={error('primaryTimeSource')} hint={`Falls back to ${SOURCE_LABEL[config.secondaryTimeSource]}.`}>
                <SelectInput
                  dataField="primaryTimeSource"
                  value={String(config.primaryTimeSource)}
                  options={[
                    { value: String(NTP), label: 'NTP' },
                    { value: String(GPS), label: 'GPS' },
                  ]}
                  onChange={(v) => {
                    const primary = parseInt(v, 10);
                    updateMany([
                      [['primaryTimeSource'], primary],
                      [['secondaryTimeSource'], primary === NTP ? GPS : NTP],
                    ]);
                  }}
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
