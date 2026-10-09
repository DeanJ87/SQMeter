import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import type { SystemStatus } from '../types';
import { useWifiScan } from '../hooks/useWifiScan';
import { Button, Card, Note } from './ui';
import { Field, TextInput } from './settings/controls';

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
        setPhase({ kind: 'failed', ssid, text: data.error || 'The device refused the request' });
        return;
      }
    } catch {
      setPhase({ kind: 'failed', ssid, text: 'Could not reach the device' });
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
        setPhase({ kind: 'failed', ssid, text: `Couldn't join ${ssid}. Check the password and that the network is in range.` });
        return;
      }
    }
    setPhase({ kind: 'failed', ssid, text: 'No answer from the device. If it joined the network, it will restart there.' });
  };

  if (phase.kind === 'connected') {
    const local = phase.mdns !== false && phase.hostname ? `http://${phase.hostname}.local` : null;
    return (
      <div class="panel-page compact-page page-enter">
        <Card title="Connected" icon="wifi" tone="green">
          <div class="card-body">
            <p>
              SQMeter joined <strong>{phase.ssid}</strong> and is restarting. Reconnect this phone or computer to{' '}
              <strong>{phase.ssid}</strong>, then open:
            </p>
            <ul class="wifi-addresses">
              {local && (
                <li>
                  <a href={local}>{local}</a>
                </li>
              )}
              <li>
                <a href={`http://${phase.ip}`}>http://{phase.ip}</a>
              </li>
            </ul>
            <Note>The SQM-Setup hotspot turns off after the restart.</Note>
          </div>
        </Card>
      </div>
    );
  }

  const connecting = phase.kind === 'connecting';
  return (
    <div class="panel-page compact-page page-enter">
      <div>
        <h2 class="page-title">WiFi setup</h2>
        <p class="muted">Choose the network SQMeter should join.</p>
      </div>
      <Card
        title="Networks"
        icon="wifi"
        actions={
          <Button small onClick={() => void scan()} busy={scanning} busyLabel="Scanning..." disabled={connecting}>
            Scan again
          </Button>
        }
      >
        <div class="card-body">
          {scanError && <Note tone="bad">{scanError}</Note>}
          {!scanning && networks.length === 0 && !scanError && <Note>No networks found.</Note>}
          <div class="wifi-list" role="radiogroup" aria-label="WiFi networks">
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
              <span>Other network...</span>
            </button>
          </div>

          {selected !== null && (
            <div class="form-grid">
              {selected === OTHER && (
                <Field label="Network name">
                  <TextInput dataField="setup.ssid" value={otherSsid} onInput={setOtherSsid} />
                </Field>
              )}
              {needsPassword && (
                <Field label="Password">
                  <TextInput dataField="setup.password" type="password" value={password} onInput={setPassword} />
                </Field>
              )}
            </div>
          )}

          {phase.kind === 'failed' && <Note tone="bad">{phase.text}</Note>}
          <div class="btn-row">
            <Button variant="primary" onClick={() => void connect()} disabled={!ssid} busy={connecting} busyLabel="Connecting...">
              Connect
            </Button>
          </div>
        </div>
      </Card>
    </div>
  );
};

export default WifiSetup;
