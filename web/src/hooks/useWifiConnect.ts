import { useState } from 'preact/hooks';
import type { SystemStatus } from '../types';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';
import { bodyOf, postJson, request } from '../lib/api';

const GIVE_UP_POLLS = 20;

export type WifiConnectPhase =
  | { kind: 'idle' }
  | { kind: 'connecting'; ssid: string }
  | { kind: 'connected'; ssid: string; ip: string; hostname?: string; mdns?: boolean }
  | { kind: 'failed'; ssid: string; text: string };

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

// Ask the device to join `ssid`; the failure text when it refuses, null when it accepted.
const requestConnect = async (ssid: string, password: string): Promise<string | null> => {
  try {
    const response = await postJson('/api/wifi/connect', { ssid, password });
    if (response.ok) return null;
    return deviceError(await bodyOf(response), t('wifiSetup.theDeviceRefusedTheRequest'));
  } catch {
    return t('wifiSetup.couldNotReachTheDevice');
  }
};

// Poll /api/status until the device is on `ssid`, gives up, or stops answering.
const awaitJoin = async (ssid: string, pollMs: number): Promise<WifiConnectPhase> => {
  let sawPending = false;
  for (let poll = 1; poll <= GIVE_UP_POLLS; poll++) {
    await sleep(pollMs);
    let status: SystemStatus;
    try {
      // The hotspot can drop for a moment while the device joins the network.
      status = await (await request('/api/status')).json();
    } catch {
      continue;
    }
    const wifi = status.wifi;
    if (wifi.connectPending) {
      sawPending = true;
      continue;
    }
    if (wifi.connected && wifi.ssid === ssid) {
      return { kind: 'connected', ssid, ip: wifi.ip, hostname: wifi.hostname, mdns: wifi.mdns };
    }
    // Not pending any more (or never seen pending after a few polls) and not
    // on the chosen network: the attempt failed.
    if (sawPending || poll >= 3) {
      return { kind: 'failed', ssid, text: t('wifiSetup.couldnTJoinSsidCheck', { ssid }) };
    }
  }
  return { kind: 'failed', ssid, text: t('wifiSetup.noAnswerFromTheDevice') };
};

// The setup page's connect flow: send the credentials, then watch the device join.
export const useWifiConnect = (pollMs: number) => {
  const [phase, setPhase] = useState<WifiConnectPhase>({ kind: 'idle' });

  const connect = async (ssid: string, password: string) => {
    setPhase({ kind: 'connecting', ssid });
    const refused = await requestConnect(ssid, password);
    if (refused !== null) {
      setPhase({ kind: 'failed', ssid, text: refused });
      return;
    }
    setPhase(await awaitJoin(ssid, pollMs));
  };

  return { phase, connect };
};
