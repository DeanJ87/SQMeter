import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import type { SystemStatus } from '../types';
import { useWifiScan } from '../hooks/useWifiScan';
import { Button, Card, Note } from './ui';
import { Field, TextInput } from './settings/controls';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

// Where the "SQM-Setup" hotspot's captive portal lands: pick a network, enter
// its password, connect. The device saves it and restarts onto that network.

const POLL_MS = 2000;
const GIVE_UP_POLLS = 20;
const OTHER = '\u0000other';

type Phase =
  | { kind: 'idle' }
  | { kind: 'connecting'; ssid: string }
  | { kind: 'connected'; ssid: string; ip: string; hostname?: string; mdns?: boolean }
  | { kind: 'failed'; ssid: string; text: string };

const WifiSetup: FunctionalComponent<{ path?: string; pollMs?: number }> = ({ pollMs = POLL_MS }) => {
  const { networks, scanning, error: scanError, scan } = useWifiScan();
  const [selected, setSelected] = useState<string | null>(null);
  const [otherSsid, setOtherSsid] = useState('');
  const [password, setPassword] = useState('');
  const [phase, setPhase] = useState<Phase>({ kind: 'idle' });

  useEffect(() => {
    void scan();
  }, []);

  const ssid = selected === OTHER ? otherSsid.trim() : (selected ?? '');
  const network = networks.find((n) => n.ssid === selected);
  const needsPassword = selected === OTHER || network?.encryption !== 'open';

  const connect = async () => {
    setPhase({ kind: 'connecting', ssid });
    try {
      const response = await fetch('/api/wifi/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password: needsPassword ? password : '' }),
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        setPhase({ kind: 'failed', ssid, text: deviceError(data, t('wifiSetup.theDeviceRefusedTheRequest')) });
        return;
      }
    } catch {
      setPhase({ kind: 'failed', ssid, text: t('wifiSetup.couldNotReachTheDevice') });
      return;
    }

    let sawPending = false;
    for (let poll = 1; poll <= GIVE_UP_POLLS; poll++) {
      await new Promise((resolve) => setTimeout(resolve, pollMs));
      let status: SystemStatus;
      try {
        // The hotspot can drop for a moment while the device joins the network.
        status = await (await fetch('/api/status')).json();
      } catch {
        continue;
      }
      const wifi = status.wifi;
      if (wifi.connectPending) {
        sawPending = true;
        continue;
      }
      if (wifi.connected && wifi.ssid === ssid) {
        setPhase({ kind: 'connected', ssid, ip: wifi.ip, hostname: wifi.hostname, mdns: wifi.mdns });
        return;
      }
      // Not pending any more (or never seen pending after a few polls) and not
      // on the chosen network: the attempt failed.
      if (sawPending || poll >= 3) {
        setPhase({ kind: 'failed', ssid, text: t('wifiSetup.couldnTJoinSsidCheck', { ssid }) });
        return;
      }
    }
    setPhase({ kind: 'failed', ssid, text: t('wifiSetup.noAnswerFromTheDevice') });
  };

  if (phase.kind === 'connected') {
    const local = phase.mdns !== false && phase.hostname ? `http://${phase.hostname}.local` : null;
    return (
      <div class="panel-page compact-page page-enter">
        <Card title={t('wifiSetup.connected')} icon="wifi" tone="green">
          <div class="card-body">
            <p>
              {t('wifiSetup.sqmeterJoined')} <strong>{phase.ssid}</strong> {t('wifiSetup.andIsRestartingReconnectThis')}{' '}
              <strong>{phase.ssid}</strong>
              {t('wifiSetup.thenOpen')}
            </p>
            <ul class="wifi-addresses">
              {local && (
                <li>
                  <a href={local}>{local}</a>
                </li>
              )}
              <li>
                <a href={`http://${phase.ip}`}>{`http://${phase.ip}`}</a>
              </li>
            </ul>
            <Note>{t('wifiSetup.theSqmSetupHotspotTurns')}</Note>
          </div>
        </Card>
      </div>
    );
  }

  const connecting = phase.kind === 'connecting';
  return (
    <div class="panel-page compact-page page-enter">
      <div>
        <h2 class="page-title">{t('wifiSetup.wifiSetup')}</h2>
        <p class="muted">{t('wifiSetup.chooseTheNetworkSqmeterShould')}</p>
      </div>
      <Card
        title={t('wifiSetup.networks')}
        icon="wifi"
        actions={
          <Button small onClick={() => void scan()} busy={scanning} busyLabel={t('common.scanning')} disabled={connecting}>
            {t('wifiSetup.scanAgain')}
          </Button>
        }
      >
        <div class="card-body">
          {scanError && <Note tone="bad">{scanError}</Note>}
          {!scanning && networks.length === 0 && !scanError && <Note>{t('wifiSetup.noNetworksFound')}</Note>}
          <div class="wifi-list" role="radiogroup" aria-label={t('wifiSetup.wifiNetworks')}>
            {networks.map((n) => (
              <button
                key={n.ssid}
                type="button"
                role="radio"
                aria-checked={selected === n.ssid}
                class={`wifi-option${selected === n.ssid ? ' is-selected' : ''}`}
                onClick={() => setSelected(n.ssid)}
                disabled={connecting}
              >
                <span>{n.ssid}</span>
                <span class="muted">
                  {n.encryption === 'secured' ? '🔒 ' : ''}
                  {n.rssi} dBm
                </span>
              </button>
            ))}
            <button
              type="button"
              role="radio"
              aria-checked={selected === OTHER}
              class={`wifi-option${selected === OTHER ? ' is-selected' : ''}`}
              onClick={() => setSelected(OTHER)}
              disabled={connecting}
            >
              <span>{t('wifiSetup.otherNetwork')}</span>
            </button>
          </div>

          {selected !== null && (
            <div class="form-grid">
              {selected === OTHER && (
                <Field label={t('wifiSetup.networkName')}>
                  <TextInput dataField="setup.ssid" value={otherSsid} onInput={setOtherSsid} />
                </Field>
              )}
              {needsPassword && (
                <Field label={t('wifiSetup.password')}>
                  <TextInput dataField="setup.password" type="password" value={password} onInput={setPassword} />
                </Field>
              )}
            </div>
          )}

          {phase.kind === 'failed' && <Note tone="bad">{phase.text}</Note>}
          <div class="btn-row">
            <Button variant="primary" onClick={() => void connect()} disabled={!ssid} busy={connecting} busyLabel={t('common.connecting')}>
              {t('wifiSetup.connect')}
            </Button>
          </div>
        </div>
      </Card>
    </div>
  );
};

export default WifiSetup;
