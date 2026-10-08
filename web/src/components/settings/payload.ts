import type { Config } from '../../types';
import {
  defaultAlpacaConfig,
  defaultAuthConfig,
  defaultBleConfig,
  defaultCloudDetectionConfig,
  defaultLocationConfig,
  defaultRainConfig,
  defaultWindConfig,
  mergeAlertsConfig,
  defaultMqttPublish,
  defaultHomeAssistant,
  defaultSkyAveraging,
  defaultSkyCalibration,
} from './defaults';

// Normalises a config from the device (or the form) into the full shape the
// UI edits and the firmware accepts, filling in fields that older firmware
// didn't have.

export const fieldErrorAliases: Record<string, string> = {
  mqttBroker: 'mqtt.broker',
  mqttPort: 'mqtt.port',
  mqttTopic: 'mqtt.topic',
  mqttInterval: 'mqtt.publishIntervalMs',
  sensorInterval: 'sensor.readIntervalMs',
  i2cSDA: 'sensor.i2cSDA',
  i2cSCL: 'sensor.i2cSCL',
  i2cPins: 'sensor.i2cSDA',
  i2cFrequency: 'sensor.i2cFrequency',
};

const isSourceEnabled = (candidate: Config, source: number) => (source === 0 ? candidate.ntp.enabled : candidate.gps.enabled);

const normalizeTimeSources = (candidate: Config): Pick<Config, 'primaryTimeSource' | 'secondaryTimeSource'> => {
  const ntpEnabled = candidate.ntp.enabled;
  const gpsEnabled = candidate.gps.enabled;

  if (!ntpEnabled && !gpsEnabled) {
    return {
      primaryTimeSource: candidate.primaryTimeSource,
      secondaryTimeSource: candidate.secondaryTimeSource,
    };
  }

  const primaryTimeSource = isSourceEnabled(candidate, candidate.primaryTimeSource) ? candidate.primaryTimeSource : ntpEnabled ? 0 : 1;

  let secondaryTimeSource = isSourceEnabled(candidate, candidate.secondaryTimeSource)
    ? candidate.secondaryTimeSource
    : gpsEnabled && primaryTimeSource !== 1
      ? 1
      : 0;

  if (ntpEnabled && gpsEnabled && primaryTimeSource === secondaryTimeSource) {
    secondaryTimeSource = primaryTimeSource === 0 ? 1 : 0;
  }

  if (!ntpEnabled || !gpsEnabled) {
    secondaryTimeSource = primaryTimeSource;
  }

  return { primaryTimeSource, secondaryTimeSource };
};

export const toConfigPayload = (source: Config): Config => {
  const rain = source.rain ? { ...source.rain } : { ...defaultRainConfig };
  const auth = source.auth ? { ...source.auth } : { ...defaultAuthConfig };
  const base: Config = {
    deviceName: source.deviceName,
    primaryTimeSource: source.primaryTimeSource,
    secondaryTimeSource: source.secondaryTimeSource,
    wifi: { ...source.wifi },
    mqtt: {
      ...source.mqtt,
      publish: { ...defaultMqttPublish, ...source.mqtt.publish },
      homeAssistant: { ...defaultHomeAssistant, ...source.mqtt.homeAssistant },
    },
    ota: { ...source.ota },
    auth,
    ntp: { ...source.ntp },
    gps: { ...source.gps },
    rain,
    sensor: { ...source.sensor },
    skyAveraging: { ...defaultSkyAveraging, ...source.skyAveraging },
    skyCalibration: { ...defaultSkyCalibration, ...source.skyCalibration },
    cloudDetection: source.cloudDetection ? { ...source.cloudDetection } : { ...defaultCloudDetectionConfig },
    // Merge defaults so configs from older firmware gain newly added fields.
    alpaca: { ...defaultAlpacaConfig, ...source.alpaca },
    alerts: mergeAlertsConfig(source.alerts),
    ble: { ...defaultBleConfig, ...source.ble },
    wind: { ...defaultWindConfig, ...source.wind },
    location: { ...defaultLocationConfig, ...source.location },
  };

  return {
    ...base,
    ...normalizeTimeSources(base),
  };
};
