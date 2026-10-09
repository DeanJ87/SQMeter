import { useState } from 'preact/hooks';
import type { AlertChannelName, AlertEventKey, AlertEventSetting, AlertRecord } from '../types';
import { bodyOf, post, request } from '../lib/api';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';
import { channelLabel, deliveryDetail, deliveryStatusLabel } from '../lib/alertDelivery';

// Test alerts from Settings > Alerts: one channel, or one event on every
// channel. The device sends in the background, so the result is followed in
// /api/alerts/recent until every channel reports sent / failed / skipped.

export type AlertTestResult = { target: string; type: 'success' | 'error' | 'pending'; text: string };

const fetchRecent = async (): Promise<AlertRecord[]> => {
  const response = await request('/api/alerts/recent');
  if (!response.ok) return [];
  const body = await response.json();
  return Array.isArray(body?.alerts) ? body.alerts : [];
};

// null while pending; a leading "!" marks a failure.
const channelResult = (channel: AlertChannelName) => (record: AlertRecord) => {
  const result = record.channels[channel];
  if (!result || result.status === 'pending') return null;
  if (result.status === 'sent') return t('settings.alerts.delivered');
  return `!${deliveryStatusLabel(result.status)}: ${deliveryDetail(result.detail)}`;
};

// One "channel: result" per channel, comma-separated (DS-24: no " · " run-on).
const channelSummary = (channel: string, r: { status: string; detail: string }) =>
  r.status === 'sent'
    ? `${channelLabel(channel)}: ${deliveryStatusLabel(r.status)}`
    : `${channelLabel(channel)}: ${deliveryStatusLabel(r.status)} (${deliveryDetail(r.detail)})`;

const allChannelsResult = (record: AlertRecord) => {
  const entries = Object.entries(record.channels) as [AlertChannelName, { status: string; detail: string }][];
  if (entries.some(([, r]) => r.status === 'pending')) return null;
  const summary = entries.map(([channel, r]) => channelSummary(channel, r)).join(', ');
  return entries.every(([, r]) => r.status === 'sent') ? summary : `!${summary}`;
};

const eventQuery = (key: AlertEventKey, event: AlertEventSetting) =>
  `channel=all&event=${key}&level=${event.level}&sound=${encodeURIComponent(event.sound)}` +
  `&title=${encodeURIComponent(event.title ?? '')}&message=${encodeURIComponent(event.message ?? '')}`;

type SetResult = (result: AlertTestResult) => void;
type Matcher = { matches: (record: AlertRecord) => boolean; pick: (record: AlertRecord) => string | null };

const followResult = async (setResult: SetResult, target: string, before: number, { matches, pick }: Matcher) => {
  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 1000));
    const record = (await fetchRecent()).find((r) => r.id > before && matches(r));
    const result = record ? pick(record) : null;
    if (result !== null) {
      const failed = result.startsWith('!');
      setResult({ target, type: failed ? 'error' : 'success', text: failed ? result.slice(1) : result });
      return;
    }
  }
  setResult({ target, type: 'error', text: t('settings.alerts.noResultFromTheDevice') });
};

const runTest = async (setResult: SetResult, target: string, query: string, matcher: Matcher) => {
  setResult({ target, type: 'pending', text: t('common.sending') });
  try {
    const before = (await fetchRecent())[0]?.id ?? 0;
    const response = await post(`/api/alerts/test?${query}`);
    if (!response.ok) {
      const body = await bodyOf(response);
      setResult({ target, type: 'error', text: deviceError(body, t('settings.alerts.testFailed')) });
      return;
    }
    await followResult(setResult, target, before, matcher);
  } catch {
    setResult({ target, type: 'error', text: t('settings.alerts.couldNotReachTheDevice') });
  }
};

// Only paired phones to ring; nothing to follow.
const ringPhones = (setResult: SetResult, key: AlertEventKey, query: string) => {
  setResult({ target: key, type: 'pending', text: t('common.sending') });
  post(`/api/alerts/test?${query}`)
    .then(async (response) => {
      const body = await bodyOf(response);
      setResult(
        response.ok
          ? { target: key, type: 'success', text: t('settings.alerts.ringingPairedPhones') }
          : { target: key, type: 'error', text: deviceError(body, t('settings.alerts.testFailed')) },
      );
    })
    .catch(() => setResult({ target: key, type: 'error', text: t('settings.alerts.couldNotReachTheDevice') }));
};

export const useAlertTest = () => {
  const [testResult, setTestResult] = useState<AlertTestResult | null>(null);

  const sendTest = (channel: AlertChannelName) =>
    runTest(setTestResult, channel, `channel=${channel}`, { matches: (r) => r.event === 'test', pick: channelResult(channel) });

  const sendEventTest = (key: AlertEventKey, event: AlertEventSetting, channelCount: number) => {
    const query = eventQuery(key, event);
    if (channelCount === 0) {
      ringPhones(setTestResult, key, query);
      return;
    }
    return runTest(setTestResult, key, query, { matches: (r) => r.event === key && r.title.startsWith('Test'), pick: allChannelsResult });
  };

  return { testResult, sendTest, sendEventTest };
};
