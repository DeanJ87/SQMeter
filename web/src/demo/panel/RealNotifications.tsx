import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { Button, Note } from '../../components/ui';
import { demoDevice } from '../device';
import { realSender, type RealChannel, type RealCredentials, type RealSender } from '../realSend';

// Demo panel section: opt in to real notifications (specs/018 US2/US3).
// Off by default; turning it on needs a confirmation that says where
// messages go and that the keys stay in this tab (FR-005).

const CONFIRM_TEXT =
  "Alerts from this demo will be sent from your browser to the services you set up below - ntfy.sh, Pushover or your own MQTT broker - with the device's own wording. Your keys stay in this tab only: they're gone when you reload or close it. At most one message per service every 30 s, 10 per visit; Wake-level alerts go as Urgent.";

type Fields = Record<string, string>;

const FIELDS: Record<RealChannel, { key: string; label: string; secret?: boolean; optional?: boolean; placeholder?: string }[]> = {
  ntfy: [
    { key: 'topic', label: 'ntfy.sh topic', placeholder: 'my-sqmeter-demo' },
    { key: 'token', label: 'Access token', secret: true, optional: true },
  ],
  pushover: [
    { key: 'userKey', label: 'Pushover user key', secret: true },
    { key: 'appToken', label: 'Pushover app token', secret: true },
  ],
  mqtt: [
    { key: 'url', label: 'Broker (secure WebSocket)', placeholder: 'wss://broker.example:8884/mqtt' },
    { key: 'topic', label: 'Base topic', placeholder: 'sqmeter' },
    { key: 'username', label: 'Username', optional: true },
    { key: 'password', label: 'Password', secret: true, optional: true },
  ],
};

const TITLES: Record<RealChannel, string> = { ntfy: 'ntfy', pushover: 'Pushover', mqtt: 'MQTT' };

const toCredentials = (channel: RealChannel, fields: Fields): RealCredentials[RealChannel] => {
  if (channel === 'ntfy') return { topic: fields.topic ?? '', token: fields.token || undefined };
  if (channel === 'pushover') return { userKey: fields.userKey ?? '', appToken: fields.appToken ?? '' };
  return {
    url: fields.url ?? '',
    topic: fields.topic || 'sqmeter',
    username: fields.username || undefined,
    password: fields.password || undefined,
  };
};

const ChannelForm: FunctionalComponent<{ sender: RealSender; channel: RealChannel }> = ({ sender, channel }) => {
  const [fields, setFields] = useState<Fields>({});
  const [problem, setProblem] = useState<string | null>(null);
  const filled = Object.values(fields).some((v) => v !== '');

  const update = (key: string, value: string) => {
    const next = { ...fields, [key]: value };
    setFields(next);
    const blank = Object.values(next).every((v) => v === '');
    setProblem(sender.setChannel(channel, blank ? null : toCredentials(channel, next)));
  };

  return (
    <fieldset class="demo-real-channel">
      <legend>
        {TITLES[channel]}
        {sender.channel(channel) ? ' - ready' : ''}
      </legend>
      {FIELDS[channel].map((f) => (
        <div class="field" key={f.key}>
          <label class="field-label" for={`real-${channel}-${f.key}`}>
            {f.label}
            {f.optional ? ' (optional)' : ''}
          </label>
          <input
            id={`real-${channel}-${f.key}`}
            class="input"
            type={f.secret ? 'password' : 'text'}
            autoComplete="off"
            spellcheck={false}
            placeholder={f.placeholder}
            value={fields[f.key] ?? ''}
            onInput={(e) => update(f.key, (e.target as HTMLInputElement).value)}
          />
        </div>
      ))}
      {filled && problem && <Note tone="warn">{problem}</Note>}
      {channel === 'ntfy' && <Note>ntfy topics are public: anyone who knows the topic can read it.</Note>}
    </fieldset>
  );
};

const LastResults: FunctionalComponent<{ sender: RealSender }> = ({ sender }) => {
  const recent = JSON.parse(demoDevice.recentAlerts()).alerts as { id: number; title: string }[];
  const latest = recent.find((a) => sender.resultsFor(a.id));
  const results = latest && sender.resultsFor(latest.id);
  if (!latest || !results) return null;
  return (
    <ul class="demo-real-results" aria-label="Last real delivery">
      {[...results].map(([channel, result]) => (
        <li key={channel} class={`tone-${result.status === 'sent' ? 'green' : result.status === 'failed' ? 'red' : 'muted'}`}>
          {latest.title} - {TITLES[channel]}: {result.detail}
        </li>
      ))}
    </ul>
  );
};

const Confirm: FunctionalComponent<{ onYes: () => void; onNo: () => void }> = ({ onYes, onNo }) => (
  <div role="group" aria-label="Confirm real notifications">
    <Note tone="warn">{CONFIRM_TEXT}</Note>
    <div class="btn-row">
      <Button small variant="primary" onClick={onYes}>
        Turn on
      </Button>
      <Button small onClick={onNo}>
        Cancel
      </Button>
    </div>
  </div>
);

const Channels: FunctionalComponent<{ sender: RealSender }> = ({ sender }) => {
  const sendTest = () => {
    demoDevice.realTestAlert();
    sender.check();
  };
  return (
    <>
      {(['ntfy', 'pushover', 'mqtt'] as RealChannel[]).map((channel) => (
        <ChannelForm key={channel} sender={sender} channel={channel} />
      ))}
      <Note>
        Webhooks and self-hosted ntfy servers need a real SQMeter: this page may only reach ntfy.sh, Pushover and secure MQTT brokers.
      </Note>
      <div class="btn-row">
        <Button small onClick={sendTest} disabled={sender.configured().length === 0}>
          Send a test
        </Button>
      </div>
      <LastResults sender={sender} />
    </>
  );
};

const RealNotifications: FunctionalComponent = () => {
  const [sender, setSender] = useState<RealSender | null>(null);
  const [confirming, setConfirming] = useState(false);
  const [, setTick] = useState(0);

  useEffect(() => {
    let unsubscribe: (() => void) | undefined;
    void realSender().then((s) => {
      setSender(s);
      unsubscribe = s.onChange(() => setTick((n) => n + 1));
    });
    return () => unsubscribe?.();
  }, []);
  if (!sender) return null;

  return (
    <section class="demo-section" aria-label="Real notifications">
      <h3>Real notifications</h3>
      <label class="demo-check">
        <input
          type="checkbox"
          checked={sender.enabled || confirming}
          onChange={(e) => toggle(sender, (e.target as HTMLInputElement).checked, setConfirming)}
        />
        Send real notifications from this demo
      </label>
      {confirming && !sender.enabled && (
        <Confirm
          onYes={() => {
            sender.setEnabled(true);
            setConfirming(false);
          }}
          onNo={() => setConfirming(false)}
        />
      )}
      {!sender.enabled && !confirming && <Note>Off: nothing leaves your browser.</Note>}
      {sender.enabled && <Channels sender={sender} />}
    </section>
  );
};

function toggle(sender: RealSender, on: boolean, setConfirming: (on: boolean) => void) {
  if (on) setConfirming(true);
  else {
    setConfirming(false);
    sender.setEnabled(false);
  }
}

export default RealNotifications;
