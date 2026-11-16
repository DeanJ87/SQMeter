import { afterEach, describe, expect, it } from 'vitest';
import es from '../locales/es.json';
import { setLanguage, type Messages } from '../index';
import { deviceText } from '../deviceMessage';
import { channelLabel, deliveryStatusLabel } from '../../lib/alertDelivery';

const spanish = es as unknown as Messages;

describe('device text in the UI language', () => {
  afterEach(() => setLanguage('en', null));

  it('translates a test alert, values included', () => {
    setLanguage('es', spanish);
    expect(deviceText('Test: Rain detected')).toBe(`Prueba: ${es['device.safety.rainDetected']}`);
    expect(deviceText('This is how a "Rain starts" alert arrives.')).toBe(`Así llega una alerta «${es['settings.alerts.rainStarts']}».`);
    expect(deviceText('Demo: nothing was sent')).toBe('Demo: no se envió nada');
  });

  it('translates the grouped sensor fault and the first drops of rain', () => {
    setLanguage('es', spanish);
    expect(deviceText('Sensor fault: several sensors')).toBe(es['device.alert.sensorFaultSeveralSensors']);
    expect(deviceText('Sensor fault: MLX90614')).toBe(es['device.alert.sensorFaultName'].replace('{name}', 'MLX90614'));
    expect(deviceText('TSL2591, MLX90614 are reporting normally again.')).toBe(
      es['device.alert.nameAreReportingNormallyAgain'].replace('{name}', 'TSL2591, MLX90614'),
    );
    expect(deviceText('The rain sensor reports the first drops of rain.')).toBe(es['device.alert.theRainSensorReportsTheFirst']);
  });

  it('leaves text it does not know as sent', () => {
    setLanguage('es', spanish);
    expect(deviceText('My own title')).toBe('My own title');
    expect(deviceText('HTTP 401')).toBe('HTTP 401');
  });

  it('labels channels and delivery results', () => {
    setLanguage('es', spanish);
    expect(channelLabel('ntfy')).toBe('ntfy');
    expect(deliveryStatusLabel('sent')).toBe('Enviada');
    expect(deliveryStatusLabel('pending')).toBe('Enviando');
  });
});
