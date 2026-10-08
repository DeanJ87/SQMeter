import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlpacaConfiguredDevice, AlpacaDeviceStateItem, AlpacaResponse, Config, SafetyStatus } from '../types';
import SafetyCard from './SafetyCard';
import { Button, Card, Note, Pill, ReadingRow } from './ui';

const POLL_INTERVAL_MS = 5000;

const alpacaGet = async <T,>(path: string): Promise<AlpacaResponse<T> | null> => {
  try {
    const response = await fetch(path);
    if (!response.ok) return null;
    return (await response.json()) as AlpacaResponse<T>;
  } catch {
    return null;
  }
};

const formatStateValue = (value: unknown): string => {
  if (typeof value === 'number') return Number.isInteger(value) ? String(value) : value.toFixed(2);
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
          {copied ? 'Copied' : 'Copy'}
        </Button>
      </span>
    </div>
  );
};

const deviceBasePath = (device: AlpacaConfiguredDevice) => `/api/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}`;

const Alpaca: FunctionalComponent = () => {
  const [config, setConfig] = useState<Config | null>(null);
  const [devices, setDevices] = useState<AlpacaConfiguredDevice[] | null>(null);
  const [deviceStates, setDeviceStates] = useState<Record<string, AlpacaDeviceStateItem[] | null>>({});
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);
  const [safety, setSafety] = useState<SafetyStatus | null>(null);

  const origin = typeof window !== 'undefined' ? window.location.origin : '';
  const host = typeof window !== 'undefined' ? window.location.hostname : '';
  const port = typeof window !== 'undefined' ? window.location.port || '80' : '80';

  useEffect(() => {
    fetch('/api/config')
      .then((response) => (response.ok ? response.json() : null))
      .then((data) => setConfig(data))
      .catch(() => setConfig(null));

    alpacaGet<AlpacaConfiguredDevice[]>('/management/v1/configureddevices').then((response) => setDevices(response?.Value ?? []));
  }, []);

  useEffect(() => {
    const pollSafety = () =>
      fetch('/api/safety')
        .then((response) => (response.ok ? response.json() : null))
        .then((data) => setSafety(data))
        .catch(() => setSafety(null));
    pollSafety();
    const timer = setInterval(pollSafety, POLL_INTERVAL_MS);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!devices || devices.length === 0) return undefined;

    const poll = async () => {
      const entries = await Promise.all(
        devices.map(async (device) => {
          const response = await alpacaGet<AlpacaDeviceStateItem[]>(`${deviceBasePath(device)}/devicestate`);
          return [device.UniqueID, response && response.ErrorNumber === 0 ? response.Value : null] as const;
        }),
      );
      setDeviceStates(Object.fromEntries(entries));
      setLastUpdated(new Date());
    };

    poll();
    const timer = setInterval(poll, POLL_INTERVAL_MS);
    return () => clearInterval(timer);
  }, [devices]);

  const enabled = config?.alpaca?.enabled ?? false;

  return (
    <div class="panel-page page-enter">
      <Card
        title="ASCOM Alpaca"
        icon="star"
        hint="N.I.N.A. and other Alpaca clients find this device by UDP discovery. If discovery can't reach it, add it manually with this host and port."
        actions={<Pill tone={enabled ? 'pill-green' : 'pill-dim'}>{enabled ? 'Enabled' : 'Disabled'}</Pill>}
      >
        <div class="card-body">
          {!enabled && (
            <Note tone="warn" action={{ label: 'Turn on', onClick: () => route('/settings?tab=safety') }}>
              Alpaca is off, so no devices are advertised.
            </Note>
          )}
          <div>
            <ReadingRow label="Host" value={host || '--'} />
            <ReadingRow label="Port" value={port} />
            <ReadingRow label="Discovery port (UDP)" value="32227" />
            <CopyableUrl label="Description" url={`${origin}/management/v1/description`} open />
            <CopyableUrl label="Devices" url={`${origin}/management/v1/configureddevices`} open />
          </div>
          <div>
            <Button variant="link" onClick={() => route('/settings?tab=safety')}>
              Alpaca and safety settings →
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
                <ReadingRow label="Type" value={`${device.DeviceType} #${device.DeviceNumber}`} />
                <ReadingRow label="Unique ID" value={device.UniqueID} />
                <CopyableUrl
                  label="Setup page"
                  url={`${origin}/setup/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}/setup`}
                  open
                />
                <CopyableUrl label="API base" url={`${origin}${base}`} />
                <CopyableUrl label="Device state" url={`${origin}${base}/devicestate`} open />
                {device.DeviceType === 'SafetyMonitor' && <CopyableUrl label="IsSafe" url={`${origin}${base}/issafe`} open />}
              </div>
              <div class="card-group">
                <h3 class="card-group-title">Live state</h3>
                {state === undefined && <Note>Loading...</Note>}
                {state === null && <Note tone="bad">Unavailable</Note>}
                {state && state.length === 0 && <Note>No values yet</Note>}
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

      {lastUpdated && <Note>Updated {lastUpdated.toLocaleTimeString()}</Note>}
    </div>
  );
};

export default Alpaca;
