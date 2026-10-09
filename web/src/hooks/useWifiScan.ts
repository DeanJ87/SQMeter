import { useEffect, useRef, useState } from 'preact/hooks';
import type { WiFiNetwork } from '../types';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

const POLL_MS = 1000;
const MAX_POLLS = 15;

// GET /api/wifi/scan starts an asynchronous scan (202, scanning: true) and
// returns the networks on a later call; poll until they arrive.
export const useWifiScan = () => {
  const [networks, setNetworks] = useState<WiFiNetwork[]>([]);
  const [scanning, setScanning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const cancelled = useRef(false);

  useEffect(
    () => () => {
      cancelled.current = true;
    },
    [],
  );

  const scan = async () => {
    setScanning(true);
    setError(null);
    try {
      for (let i = 0; i < MAX_POLLS && !cancelled.current; i++) {
        const response = await fetch('/api/wifi/scan');
        const data = await response.json();
        if (!response.ok) throw new Error(deviceError(data, t('wifiScan.scanFailed')));
        if (!data.scanning) {
          const seen = new Map<string, WiFiNetwork>();
          for (const n of (data.networks || []) as WiFiNetwork[]) {
            if (!n.ssid) continue;
            const prev = seen.get(n.ssid);
            if (!prev || n.rssi > prev.rssi) seen.set(n.ssid, n);
          }
          setNetworks([...seen.values()].sort((a, b) => b.rssi - a.rssi));
          return;
        }
        await new Promise((resolve) => setTimeout(resolve, POLL_MS));
      }
      if (!cancelled.current) setError(t('wifiScan.scanTimedOut'));
    } catch (e) {
      if (!cancelled.current) setError(e instanceof Error ? e.message : t('wifiScan.scanFailed'));
    } finally {
      if (!cancelled.current) setScanning(false);
    }
  };

  return { networks, scanning, error, scan };
};
