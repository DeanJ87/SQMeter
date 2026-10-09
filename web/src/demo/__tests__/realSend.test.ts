import { describe, expect, it, vi } from 'vitest';
import { MAX_PER_TAB, MIN_INTERVAL_MS, RealSender, validateChannel, withRealResults, type AlertSource } from '../realSend';

const NTFY_REQUEST = {
  ntfy: {
    url: 'https://ntfy.sh/topic-1',
    contentType: 'text/plain; charset=utf-8',
    headers: [
      ['Title', 'SQMeter Demo: Rain detected'],
      ['Priority', 'high'],
    ],
    body: 'Rain',
  },
};

// A fake device: alerts are recorded with increasing ids.
function fakeSource() {
  const ids: number[] = [];
  const source: AlertSource & { add: () => void; requested: string[] } = {
    requested: [],
    add: () => ids.push(ids.length + 1),
    recentAlerts: () => JSON.stringify({ alerts: [...ids].reverse().map((id) => ({ id, channels: {} })) }),
    deliveryRequests: (_id, credentials) => {
      source.requested.push(credentials);
      return JSON.stringify(JSON.parse(credentials).ntfy ? NTFY_REQUEST : {});
    },
  };
  return source;
}

function setup(response: Response = new Response('{}', { status: 200 })) {
  const source = fakeSource();
  let now = 1_000_000;
  const fetch = vi.fn().mockResolvedValue(response);
  const sender = new RealSender(source, { fetch, mqtt: vi.fn().mockResolvedValue('Published'), now: () => now });
  const advance = (ms: number) => (now += ms);
  return { source, sender, fetch, advance };
}

const flush = () => new Promise((resolve) => setTimeout(resolve, 0));

describe('RealSender', () => {
  it('sends nothing while off, even with a channel set up', async () => {
    const { source, sender, fetch } = setup();
    sender.setChannel('ntfy', { topic: 'topic-1' });
    source.add();
    sender.check();
    await flush();
    expect(fetch).not.toHaveBeenCalled();
  });

  it('only sends alerts recorded after it was turned on', async () => {
    const { source, sender, fetch } = setup();
    source.add();
    sender.setChannel('ntfy', { topic: 'topic-1' });
    sender.setEnabled(true);
    sender.check();
    await flush();
    expect(fetch).not.toHaveBeenCalled();
    source.add();
    sender.check();
    await flush();
    expect(fetch).toHaveBeenCalledTimes(1);
    expect(fetch.mock.calls[0][0]).toBe('https://ntfy.sh/topic-1');
    expect(fetch.mock.calls[0][1].headers).toContainEqual(['Priority', 'high']);
    expect(sender.resultsFor(2)?.get('ntfy')).toEqual({ status: 'sent', detail: 'Delivered' });
  });

  it('limits each channel to one per 30 s and 10 per visit', async () => {
    const { source, sender, fetch, advance } = setup();
    sender.setChannel('ntfy', { topic: 'topic-1' });
    sender.setEnabled(true);
    source.add();
    source.add();
    sender.check();
    await flush();
    expect(fetch).toHaveBeenCalledTimes(1);
    expect(sender.resultsFor(2)?.get('ntfy')?.status).toBe('skipped');
    for (let i = 1; i < MAX_PER_TAB + 2; i++) {
      advance(MIN_INTERVAL_MS);
      source.add();
      sender.check();
    }
    await flush();
    expect(fetch).toHaveBeenCalledTimes(MAX_PER_TAB);
  });

  it("shows the service's own error in plain words", async () => {
    const { source, sender } = setup(new Response(JSON.stringify({ errors: ['user identifier is invalid'] }), { status: 400 }));
    sender.setChannel('ntfy', { topic: 'topic-1' });
    sender.setEnabled(true);
    source.add();
    sender.check();
    await flush();
    await flush();
    expect(sender.resultsFor(1)?.get('ntfy')).toEqual({ status: 'failed', detail: 'user identifier is invalid' });
  });

  it('reset forgets keys, results and limits', async () => {
    const { source, sender } = setup();
    sender.setChannel('ntfy', { topic: 'topic-1' });
    sender.setEnabled(true);
    source.add();
    sender.check();
    await flush();
    sender.reset();
    expect(sender.enabled).toBe(false);
    expect(sender.configured()).toEqual([]);
    expect(sender.resultsFor(1)).toBeUndefined();
  });

  it('merges real results into the alert list', async () => {
    const { source, sender } = setup();
    sender.setChannel('ntfy', { topic: 'topic-1' });
    sender.setEnabled(true);
    source.add();
    sender.check();
    await flush();
    const merged = JSON.parse(withRealResults(source.recentAlerts(), sender));
    expect(merged.alerts[0].channels.ntfy).toEqual({ status: 'sent', detail: 'Delivered' });
  });
});

describe('validateChannel', () => {
  it.each([
    ['ntfy', { topic: 'ok_topic-1' }, null],
    ['ntfy', { topic: 'has space' }, 'Topic'],
    ['pushover', { userKey: 'u'.repeat(30), appToken: 'a'.repeat(30) }, null],
    ['pushover', { userKey: 'short', appToken: 'a'.repeat(30) }, 'User key'],
    ['mqtt', { url: 'wss://broker.example:8884/mqtt', topic: 'sqmeter' }, null],
    ['mqtt', { url: 'ws://broker.example', topic: 'sqmeter' }, 'wss://'],
  ] as const)('%s %o', (channel, creds, problem) => {
    const result = validateChannel(channel, creds as never);
    if (problem === null) expect(result).toBeNull();
    else expect(result).toContain(problem);
  });
});
