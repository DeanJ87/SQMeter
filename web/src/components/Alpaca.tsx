import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { route } from 'preact-router';
import { CLIENT_PILL, describeClient } from '../lib/alpacaClients';
import { useAlpacaClients, type AlpacaClients } from '../hooks/useAlpacaClients';
import { deviceBasePath, useAlpacaDevices } from '../hooks/useAlpacaDevices';
import SafetyCard from './SafetyCard';
import { Button, Card, Note, Pill, ReadingRow } from './ui';
import { t } from '../i18n';
import { formatCount, formatNumber, formatTime } from '../i18n/format';

const POLL_INTERVAL_MS = 5000;

const formatStateValue = (value: unknown): string => {
  if (typeof value === 'number') return Number.isInteger(value) ? formatCount(value) : formatNumber(value, 2);
  if (typeof value === 'boolean') return value ? 'true' : 'false';
  return String(value);
};

const CopyableUrl: FunctionalComponent<{ label: string; url: string; open?: boolean }> = ({ label, url, open }) => {
  const [copied, setCopied] = useState(false);
  const copy = async () => {
    try {
      await navigator.clipboard.writeText(url);
      setCopied(true);
      setTimeout(() => setCopied(false), 1500);
    } catch {
      setCopied(false);
    }
  };

  return (
    <div class="reading-row url-row">
      <span class="reading-label">{label}</span>
      <span class="url-value">
        {open ? (
          <a href={url} target="_blank" rel="noreferrer">
            {url}
          </a>
        ) : (
          <code>{url}</code>
        )}
        <Button variant="ghost" small onClick={copy}>
          {copied ? t('alpaca.copied') : t('alpaca.copy')}
        </Button>
      </span>
    </div>
  );
};

// Whether an imaging app is checking each device (specs/021).
const ImagingAppState: FunctionalComponent<{ clients: AlpacaClients | null; enabled: boolean }> = ({ clients, enabled }) =>
  enabled && clients ? (
    <div class="card-group">
      <h3 class="card-group-title">{t('alpaca.imagingApp')}</h3>
      {(
        [
          [t('alpaca.safetyMonitor'), clients.safetymonitor],
          [t('alpaca.weatherDevice'), clients.observingconditions],
        ] as const
      ).map(([label, state]) => {
        const view = describeClient(state);
        return (
          <div class="reading-row" key={label}>
            <span class="reading-label">{label}</span>
            <span class="client-state">
              {view.checked && <span class="system-subtle">{view.checked}</span>}
              <Pill tone={CLIENT_PILL[view.tone]}>{view.state}</Pill>
            </span>
          </div>
        );
      })}
    </div>
  ) : null;

const Alpaca: FunctionalComponent = () => {
  const { config, devices, deviceStates, lastUpdated, safety } = useAlpacaDevices(POLL_INTERVAL_MS);
  const clients = useAlpacaClients(POLL_INTERVAL_MS);

  const origin = typeof window !== 'undefined' ? window.location.origin : '';
  const host = typeof window !== 'undefined' ? window.location.hostname : '';
  const port = typeof window !== 'undefined' ? window.location.port || '80' : '80';

  const enabled = config?.alpaca?.enabled ?? false;

  return (
    <div class="panel-page page-enter">
      <Card
        title="ASCOM Alpaca"
        icon="star"
        hint={t('alpaca.nINAAnd')}
        actions={<Pill tone={enabled ? 'pill-green' : 'pill-dim'}>{enabled ? t('alpaca.enabled') : t('alpaca.disabled')}</Pill>}
      >
        <div class="card-body">
          {!enabled && (
            <Note tone="warn" action={{ label: t('alpaca.turnOn'), onClick: () => route('/settings?tab=safety') }}>
              {t('alpaca.alpacaIsOffSoNo')}
            </Note>
          )}
          <div>
            <ReadingRow label={t('alpaca.host')} value={host || '--'} />
            <ReadingRow label={t('alpaca.port')} value={port} />
            <ReadingRow label={t('alpaca.discoveryPortUdp')} value="32227" />
            <CopyableUrl label={t('alpaca.description')} url={`${origin}/management/v1/description`} open />
            <CopyableUrl label={t('alpaca.devices')} url={`${origin}/management/v1/configureddevices`} open />
          </div>
          <ImagingAppState clients={clients} enabled={enabled} />
          <div>
            <Button variant="link" onClick={() => route('/settings?tab=safety')}>
              {t('alpaca.alpacaAndSafetySettings')}
            </Button>
          </div>
        </div>
      </Card>

      <SafetyCard safety={safety} />

      {devices?.map((device) => {
        const base = deviceBasePath(device);
        const state = deviceStates[device.UniqueID];
        return (
          <Card key={device.UniqueID} title={device.DeviceName} icon={device.DeviceType === 'SafetyMonitor' ? 'eye' : 'cloud'}>
            <div class="card-body">
              <div>
                <ReadingRow label={t('alpaca.type')} value={`${device.DeviceType} #${device.DeviceNumber}`} />
                <ReadingRow label={t('alpaca.uniqueId')} value={device.UniqueID} />
                <CopyableUrl
                  label={t('alpaca.setupPage')}
                  url={`${origin}/setup/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}/setup`}
                  open
                />
                <CopyableUrl label={t('alpaca.apiBase')} url={`${origin}${base}`} />
                <CopyableUrl label={t('alpaca.deviceState')} url={`${origin}${base}/devicestate`} open />
                {device.DeviceType === 'SafetyMonitor' && <CopyableUrl label="IsSafe" url={`${origin}${base}/issafe`} open />}
              </div>
              <div class="card-group">
                <h3 class="card-group-title">{t('alpaca.liveState')}</h3>
                {state === undefined && <Note>{t('common.loading')}</Note>}
                {state === null && <Note tone="bad">{t('alpaca.unavailable')}</Note>}
                {state && state.length === 0 && <Note>{t('alpaca.noValuesYet')}</Note>}
                {state && (
                  <div>
                    {state.map((item) => (
                      <ReadingRow
                        key={item.Name}
                        label={item.Name}
                        value={formatStateValue(item.Value)}
                        valueClass={item.Name === 'IsSafe' ? (item.Value ? 'tone-green' : 'tone-red') : ''}
                      />
                    ))}
                  </div>
                )}
              </div>
            </div>
          </Card>
        );
      })}

      {lastUpdated && <Note>{t('alpaca.updatedLocaletimestring', { localeTimeString: formatTime(lastUpdated) })}</Note>}
    </div>
  );
};

export default Alpaca;
