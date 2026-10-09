import type { AlertChannelName } from '../types';
import { t } from '../i18n';
import { deviceText } from '../i18n/deviceMessage';

// How a channel and its delivery result read in the UI. The device reports
// both as plain words ("ntfy", "sent"); these are the UI's own, translated.

export const channelLabel = (channel: string) => {
  const names: Record<AlertChannelName, string> = {
    pushover: 'Pushover',
    ntfy: 'ntfy',
    webhook: t('settings.alerts.webhook'),
    mqtt: 'MQTT',
  };
  return names[channel as AlertChannelName] ?? channel;
};

export const deliveryStatusLabel = (status: string | undefined) => {
  switch (status) {
    case 'sent':
      return t('alerts.delivery.sent');
    case 'failed':
      return t('settings.alerts.failed');
    case 'skipped':
      return t('settings.alerts.skipped');
    default:
      return t('alerts.delivery.sending');
  }
};

/** The device's detail for a result ("MQTT is off", "HTTP 401"), translated where it can be. */
export const deliveryDetail = (detail: string | undefined) => deviceText(detail);
