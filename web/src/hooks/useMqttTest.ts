import { useState } from 'preact/hooks';
import type { Config } from '../types';
import { postJson } from '../lib/api';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

// Settings > Network "Test connection": the device tries the broker in the
// form (saved or not) and reports back.

export const useMqttTest = (config: Config) => {
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttResult, setMqttResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const testMqtt = async () => {
    setTestingMqtt(true);
    setMqttResult(null);
    try {
      const response = await postJson('/api/mqtt/test', {
        broker: config.mqtt.broker,
        port: config.mqtt.port,
        username: config.mqtt.username,
        password: config.mqtt.password,
        clientId: `SQM-${config.deviceName || 'ESP32'}-Test`,
      });
      const result = await response.json();
      setMqttResult(
        result.success
          ? { type: 'success', text: result.message || t('settings.network.connected') }
          : { type: 'error', text: deviceError(result, t('settings.network.connectionFailed')) },
      );
    } catch {
      setMqttResult({ type: 'error', text: t('settings.network.couldNotReachTheDevice') });
    } finally {
      setTestingMqtt(false);
    }
  };

  return { testingMqtt, mqttResult, testMqtt };
};
