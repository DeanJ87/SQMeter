import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlpacaConfiguredDevice, AlpacaDeviceStateItem, AlpacaResponse, Config } from '../types';
import { Card, Pill, ReadingRow } from './ui';

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
    <div class="flex flex-wrap items-center justify-between gap-2 py-1.5 border-b border-gray-700/60 last:border-b-0">
      <span class="text-sm text-gray-400">{label}</span>
      <span class="flex items-center gap-2 min-w-0">
        {open ? (
          <a href={url} target="_blank" rel="noreferrer" class="font-mono text-xs text-cyan-300 hover:underline break-all">{url}</a>
        ) : (
          <code class="font-mono text-xs text-gray-200 break-all">{url}</code>
        )}
        <button type="button" onClick={copy} class="text-xs px-2 py-0.5 rounded bg-gray-700 hover:bg-gray-600 text-gray-200">
          {copied ? 'Copied' : 'Copy'}
        </button>
      </span>
    </div>
  );
};

const deviceBasePath = (device: AlpacaConfiguredDevice) =>
  `/api/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}`;

const Alpaca: FunctionalComponent = () => {
  const [config, setConfig] = useState<Config | null>(null);
  const [devices, setDevices] = useState<AlpacaConfiguredDevice[] | null>(null);
  const [deviceStates, setDeviceStates] = useState<Record<string, AlpacaDeviceStateItem[] | null>>({});
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  const origin = typeof window !== 'undefined' ? window.location.origin : '';
  const host = typeof window !== 'undefined' ? window.location.hostname : '';
  const port = typeof window !== 'undefined' ? window.location.port || '80' : '80';

  useEffect(() => {
    fetch('/api/config')
      .then((response) => (response.ok ? response.json() : null))
      .then((data) => setConfig(data))
      .catch(() => setConfig(null));

    alpacaGet<AlpacaConfiguredDevice[]>('/management/v1/configureddevices').then((response) =>
      setDevices(response?.Value ?? [])
    );
  }, []);

  useEffect(() => {
    if (!devices || devices.length === 0) return undefined;

    const poll = async () => {
      const entries = await Promise.all(
        devices.map(async (device) => {
          const response = await alpacaGet<AlpacaDeviceStateItem[]>(`${deviceBasePath(device)}/devicestate`);
          return [device.UniqueID, response && response.ErrorNumber === 0 ? response.Value : null] as const;
        })
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
    <div class="space-y-6">
      <Card
        title="ASCOM Alpaca"
        icon="star"
        actions={<Pill tone={enabled ? 'pill-green' : 'pill-dim'}>{enabled ? 'Enabled' : 'Disabled'}</Pill>}
      >
        <p class="text-sm text-gray-400 mb-4">
          This device serves ASCOM Alpaca natively. N.I.N.A. and other Alpaca clients normally find it automatically via UDP
          discovery; if discovery can't cross your network, add the device manually using the host and port below.
        </p>
        <ReadingRow label="Host" value={host || '--'} />
        <ReadingRow label="Alpaca port (HTTP)" value={port} />
        <ReadingRow label="Discovery port (UDP)" value="32227" />
        <div class="mt-3">
          <CopyableUrl label="Server description" url={`${origin}/management/v1/description`} open />
          <CopyableUrl label="Configured devices" url={`${origin}/management/v1/configureddevices`} open />
        </div>
        {!enabled && (
          <p class="mt-4 text-sm text-amber-300">
            Alpaca is disabled, so no devices are advertised.{' '}
            <button type="button" class="underline" onClick={() => route('/settings?section=alpaca')}>
              Enable it in Settings
            </button>{' '}
            and restart to start discovery.
          </p>
        )}
        <div class="mt-4">
          <button
            type="button"
            onClick={() => route('/settings?section=alpaca')}
            class="px-3 py-1.5 rounded bg-blue-600 hover:bg-blue-500 text-white text-sm"
          >
            Alpaca &amp; safety settings
          </button>
        </div>
      </Card>

      {devices?.map((device) => {
        const base = deviceBasePath(device);
        const state = deviceStates[device.UniqueID];
        return (
          <Card key={device.UniqueID} title={device.DeviceName} icon={device.DeviceType === 'SafetyMonitor' ? 'eye' : 'cloud'}>
            <ReadingRow label="Device type" value={device.DeviceType} />
            <ReadingRow label="Device number" value={String(device.DeviceNumber)} />
            <ReadingRow label="Unique ID" value={device.UniqueID} valueClass="font-mono text-xs" />
            <div class="mt-3">
              <CopyableUrl label="Setup page" url={`${origin}/setup/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}/setup`} open />
              <CopyableUrl label="Device API base" url={`${origin}${base}`} />
              <CopyableUrl label="Device state" url={`${origin}${base}/devicestate`} open />
              {device.DeviceType === 'SafetyMonitor' && <CopyableUrl label="IsSafe" url={`${origin}${base}/issafe`} open />}
            </div>
            <h3 class="mt-4 mb-2 text-sm font-semibold text-gray-300">Live device state</h3>
            {state === undefined && <p class="text-sm text-gray-500">Loading...</p>}
            {state === null && <p class="text-sm text-red-300">Device state unavailable</p>}
            {state && state.length === 0 && <p class="text-sm text-gray-500">No values available yet</p>}
            {state &&
              state.map((item) => (
                <ReadingRow
                  key={item.Name}
                  label={item.Name}
                  value={formatStateValue(item.Value)}
                  valueClass={item.Name === 'IsSafe' ? (item.Value ? 'tone-green' : 'tone-red') : ''}
                />
              ))}
          </Card>
        );
      })}

      {lastUpdated && <p class="text-xs text-gray-500">Last updated {lastUpdated.toLocaleTimeString()}</p>}
    </div>
  );
};

export default Alpaca;
