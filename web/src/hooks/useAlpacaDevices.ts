import { useEffect, useState } from 'preact/hooks';
import type { AlpacaConfiguredDevice, AlpacaDeviceStateItem, AlpacaResponse, Config, SafetyStatus } from '../types';
import { getJson } from '../lib/api';

const alpacaGet = async <T>(path: string): Promise<AlpacaResponse<T> | null> => {
  try {
    return await getJson<AlpacaResponse<T>>(path);
  } catch {
    return null;
  }
};

export const deviceBasePath = (device: AlpacaConfiguredDevice) => `/api/v1/${device.DeviceType.toLowerCase()}/${device.DeviceNumber}`;

// What the Alpaca page shows: the config (is Alpaca on), the configured
// devices and each one's live state, and the safety verdict, polled.
export const useAlpacaDevices = (intervalMs: number) => {
  const [config, setConfig] = useState<Config | null>(null);
  const [devices, setDevices] = useState<AlpacaConfiguredDevice[] | null>(null);
  const [deviceStates, setDeviceStates] = useState<Record<string, AlpacaDeviceStateItem[] | null>>({});
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);
  const [safety, setSafety] = useState<SafetyStatus | null>(null);

  useEffect(() => {
    getJson<Config>('/api/config')
      .then((data) => setConfig(data))
      .catch(() => setConfig(null));

    alpacaGet<AlpacaConfiguredDevice[]>('/management/v1/configureddevices').then((response) => setDevices(response?.Value ?? []));
  }, []);

  useEffect(() => {
    const pollSafety = () =>
      getJson<SafetyStatus>('/api/safety')
        .then((data) => setSafety(data))
        .catch(() => setSafety(null));
    pollSafety();
    const timer = setInterval(pollSafety, intervalMs);
    return () => clearInterval(timer);
  }, [intervalMs]);

  useEffect(() => {
    if (!devices || devices.length === 0) return undefined;

    const poll = async () => {
      const entries = await Promise.all(
        devices.map(async (device) => {
          // source=ui: this page isn't an imaging app watching the device.
          const response = await alpacaGet<AlpacaDeviceStateItem[]>(`${deviceBasePath(device)}/devicestate?source=ui`);
          return [device.UniqueID, response && response.ErrorNumber === 0 ? response.Value : null] as const;
        }),
      );
      setDeviceStates(Object.fromEntries(entries));
      setLastUpdated(new Date());
    };

    poll();
    const timer = setInterval(poll, intervalMs);
    return () => clearInterval(timer);
  }, [devices, intervalMs]);

  return { config, devices, deviceStates, lastUpdated, safety };
};
