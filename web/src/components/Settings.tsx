import { FunctionalComponent } from 'preact';
import { useState, useEffect, useRef } from 'preact/hooks';
import type { Config, WiFiNetwork } from '../types';
import {
  getConfigValidationErrors,
  getConfigValidationMessage,
  hasConfigValidationErrors,
  type ValidationErrors,
} from '../validation/configSchema';

// Common timezone options with POSIX format
const TIMEZONE_OPTIONS = [
  { label: 'UTC', value: 'UTC0' },
  { label: 'US/Pacific (PST)', value: 'PST8PDT,M3.2.0,M11.1.0' },
  { label: 'US/Mountain (MST)', value: 'MST7MDT,M3.2.0,M11.1.0' },
  { label: 'US/Central (CST)', value: 'CST6CDT,M3.2.0,M11.1.0' },
  { label: 'US/Eastern (EST)', value: 'EST5EDT,M3.2.0,M11.1.0' },
  { label: 'Europe/London (GMT)', value: 'GMT0BST,M3.5.0/1,M10.5.0' },
  { label: 'Europe/Paris (CET)', value: 'CET-1CEST,M3.5.0,M10.5.0/3' },
  { label: 'Australia/Sydney (AEST)', value: 'AEST-10AEDT,M10.1.0,M4.1.0/3' },
  { label: 'Asia/Tokyo (JST)', value: 'JST-9' },
  { label: 'Custom', value: 'custom' },
];

const defaultAuthConfig: NonNullable<Config['auth']> = {
  enabled: false,
  username: 'admin',
  password: '',
};

const defaultRainConfig: NonNullable<Config['rain']> = {
  enabled: false,
  rxPin: 18,
  txPin: 19,
  baudRate: 9600,
  debugUart: false,
  mode: 'polling',
  resolution: 'high',
  units: 'metric',
  pollIntervalMs: 5000,
  rainClearDelayMs: 15 * 60 * 1000,
  dailyResetEnabled: false,
  dailyResetHour: 0,
  dailyResetMinute: 0,
};

const defaultCloudDetectionConfig: Config['cloudDetection'] = {
  clearSkyThreshold: -13.0,
  cloudyThreshold: -3.0,
  humidityCorrection: 0.75,
};

const fieldErrorAliases: Record<string, string> = {
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

const isSourceEnabled = (candidate: Config, source: number) => (
  source === 0 ? candidate.ntp.enabled : candidate.gps.enabled
);

const normalizeTimeSources = (candidate: Config): Pick<Config, 'primaryTimeSource' | 'secondaryTimeSource'> => {
  const ntpEnabled = candidate.ntp.enabled;
  const gpsEnabled = candidate.gps.enabled;

  if (!ntpEnabled && !gpsEnabled) {
    return {
      primaryTimeSource: candidate.primaryTimeSource,
      secondaryTimeSource: candidate.secondaryTimeSource,
    };
  }

  let primaryTimeSource = isSourceEnabled(candidate, candidate.primaryTimeSource)
    ? candidate.primaryTimeSource
    : ntpEnabled ? 0 : 1;

  let secondaryTimeSource = isSourceEnabled(candidate, candidate.secondaryTimeSource)
    ? candidate.secondaryTimeSource
    : gpsEnabled && primaryTimeSource !== 1 ? 1 : 0;

  if (ntpEnabled && gpsEnabled && primaryTimeSource === secondaryTimeSource) {
    secondaryTimeSource = primaryTimeSource === 0 ? 1 : 0;
  }

  if (!ntpEnabled || !gpsEnabled) {
    secondaryTimeSource = primaryTimeSource;
  }

  return { primaryTimeSource, secondaryTimeSource };
};

const toConfigPayload = (source: Config): Config => {
  const rain = source.rain ? { ...source.rain } : { ...defaultRainConfig };
  const auth = source.auth ? { ...source.auth } : { ...defaultAuthConfig };
  const base: Config = {
    deviceName: source.deviceName,
    timezone: source.timezone,
    primaryTimeSource: source.primaryTimeSource,
    secondaryTimeSource: source.secondaryTimeSource,
    wifi: { ...source.wifi },
    mqtt: { ...source.mqtt },
    ota: { ...source.ota },
    auth,
    ntp: { ...source.ntp },
    gps: { ...source.gps },
    rain,
    sensor: { ...source.sensor },
    cloudDetection: source.cloudDetection
      ? { ...source.cloudDetection }
      : { ...defaultCloudDetectionConfig },
  };

  return {
    ...base,
    ...normalizeTimeSources(base),
  };
};

const Settings: FunctionalComponent = () => {
  const [config, setConfig] = useState<Config | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [wifiNetworks, setWifiNetworks] = useState<WiFiNetwork[]>([]);
  const [scanningWifi, setScanningWifi] = useState(false);
  const [validationErrors, setValidationErrors] = useState<ValidationErrors>({});
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttTestResult, setMqttTestResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [testingRain, setTestingRain] = useState(false);
  const [rainTestResult, setRainTestResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [originalWifiSsid, setOriginalWifiSsid] = useState<string | null>(null);
  const errorPanelRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const response = await fetch('/api/config');
      const data = await response.json();
      setConfig(toConfigPayload(data));
      setOriginalWifiSsid(data.wifi?.ssid ?? '');
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to load configuration' });
    } finally {
      setLoading(false);
    }
  };

  const scanWifiNetworks = async () => {
    setScanningWifi(true);
    try {
      const response = await fetch('/api/wifi/scan');
      const data = await response.json();
      // API returns {networks: [...]} so extract the networks array
      setWifiNetworks(data.networks || []);
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to scan WiFi networks' });
    } finally {
      setScanningWifi(false);
    }
  };

  const testMqttConnection = async () => {
    if (!config) return;

    setTestingMqtt(true);
    setMqttTestResult(null);

    try {
      const response = await fetch('/api/mqtt/test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          broker: config.mqtt.broker,
          port: config.mqtt.port,
          username: config.mqtt.username,
          password: config.mqtt.password,
          clientId: `SQM-${config.deviceName || 'ESP32'}-Test`
        }),
      });

      const result = await response.json();
      
      if (result.success) {
        setMqttTestResult({ type: 'success', text: result.message || 'Connection successful!' });
      } else {
        setMqttTestResult({ type: 'error', text: result.error || 'Connection failed' });
      }
    } catch (error) {
      setMqttTestResult({ type: 'error', text: 'Network error occurred' });
    } finally {
      setTestingMqtt(false);
    }
  };

  const testRg15Communication = async () => {
    if (!config?.rain?.enabled) return;

    setTestingRain(true);
    setRainTestResult(null);

    try {
      const response = await fetch('/api/sensors/rg15/test', { method: 'POST' });
      const result = await response.json();
      if (response.ok && result.ok) {
        setRainTestResult({
          type: 'success',
          text: result.raw_response
            ? `Communication succeeded: ${result.raw_response}`
            : 'Communication succeeded',
        });
      } else {
        setRainTestResult({
          type: 'error',
          text: result.error || result.hint || 'RG-15 communication test failed',
        });
      }
    } catch (error) {
      setRainTestResult({ type: 'error', text: 'Network error occurred' });
    } finally {
      setTestingRain(false);
    }
  };

  const collectValidationErrors = (candidate: Config | null): ValidationErrors => {
    if (!candidate) return { config: 'Configuration is not loaded' };
    return getConfigValidationErrors(toConfigPayload(candidate));
  };

  const focusFirstError = (errors: ValidationErrors) => {
    const firstField = Object.keys(errors)[0];
    if (!firstField) return;

    setTimeout(() => {
      const alias = Object.entries(fieldErrorAliases).find(([, path]) => path === firstField)?.[0];
      const target = document.querySelector<HTMLElement>(`[data-field="${firstField}"]`)
        ?? (alias ? document.querySelector<HTMLElement>(`[data-field="${alias}"]`) : null)
        ?? errorPanelRef.current;

      target?.scrollIntoView({ behavior: 'smooth', block: 'center' });
      if (target instanceof HTMLInputElement || target instanceof HTMLSelectElement || target instanceof HTMLTextAreaElement) {
        target.focus({ preventScroll: true });
      }
    }, 50);
  };

  const saveConfig = async () => {
    if (!config) return;

    const payload = toConfigPayload(config);
    const errors = collectValidationErrors(payload);
    setValidationErrors(errors);
    if (hasConfigValidationErrors(errors)) {
      setMessage({ type: 'error', text: getConfigValidationMessage(errors) });
      focusFirstError(errors);
      return;
    }

    setSaving(true);
    setMessage(null);

    try {
      const response = await fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });

      const contentType = response.headers.get('content-type') || '';
      const responseBody = contentType.includes('application/json')
        ? await response.json()
        : null;

      if (response.ok) {
        if (responseBody?.success === false) {
          setMessage({ type: 'error', text: responseBody.error || 'Failed to save configuration' });
        } else {
          setMessage({ type: 'success', text: 'Configuration saved successfully!' });
          setConfig(payload);
          setValidationErrors({});
        }
      } else {
        setMessage({ type: 'error', text: responseBody?.error || 'Failed to save configuration' });
      }
    } catch (error) {
      setMessage({ type: 'error', text: 'Network error occurred' });
    } finally {
      setSaving(false);
    }
  };

  const updateConfig = (path: string[], value: any) => {
    if (!config) return;

    const newConfig = {
      ...config,
      wifi: { ...config.wifi },
      mqtt: { ...config.mqtt },
      ntp: { ...config.ntp },
      gps: { ...config.gps },
      sensor: { ...config.sensor },
      cloudDetection: { ...(config.cloudDetection ?? defaultCloudDetectionConfig) },
      auth: config.auth ? { ...config.auth } : { ...defaultAuthConfig },
      rain: config.rain ? { ...config.rain } : { ...defaultRainConfig },
    };
    let current: any = newConfig;
    
    for (let i = 0; i < path.length - 1; i++) {
      current = current[path[i]];
    }
    
    current[path[path.length - 1]] = value;
    const normalizedConfig = toConfigPayload(newConfig);
    setConfig(normalizedConfig);
    setValidationErrors(collectValidationErrors(normalizedConfig));
  };

  const updateDailyResetTime = (value: string) => {
    if (!config) return;
    const [hourRaw, minuteRaw] = value.split(':').map((part) => parseInt(part, 10));
    const hour = Number.isFinite(hourRaw) ? hourRaw : 0;
    const minute = Number.isFinite(minuteRaw) ? minuteRaw : 0;
    const newConfig = {
      ...config,
      wifi: { ...config.wifi },
      mqtt: { ...config.mqtt },
      ntp: { ...config.ntp },
      gps: { ...config.gps },
      sensor: { ...config.sensor },
      cloudDetection: { ...(config.cloudDetection ?? defaultCloudDetectionConfig) },
      auth: config.auth ? { ...config.auth } : { ...defaultAuthConfig },
      rain: {
        ...(config.rain ? { ...config.rain } : { ...defaultRainConfig }),
        dailyResetHour: hour,
        dailyResetMinute: minute,
      },
    };
    const normalizedConfig = toConfigPayload(newConfig);
    setConfig(normalizedConfig);
    setValidationErrors(collectValidationErrors(normalizedConfig));
  };

  const errorFor = (key: string) => validationErrors[fieldErrorAliases[key] ?? key];
  const shouldShowWifiPassword = Boolean(
    config && originalWifiSsid !== null && wifiNetworks.length > 0 && config.wifi.ssid !== originalWifiSsid
  );

  const updateWifiNetwork = (ssid: string) => {
    if (!config) return;
    const newConfig = {
      ...config,
      wifi: {
        ...config.wifi,
        ssid,
        password: ssid !== originalWifiSsid ? '' : config.wifi.password,
      },
      mqtt: { ...config.mqtt },
      ntp: { ...config.ntp },
      gps: { ...config.gps },
      sensor: { ...config.sensor },
      cloudDetection: { ...(config.cloudDetection ?? defaultCloudDetectionConfig) },
      auth: config.auth ? { ...config.auth } : { ...defaultAuthConfig },
      rain: config.rain ? { ...config.rain } : { ...defaultRainConfig },
    };
    const normalizedConfig = toConfigPayload(newConfig);
    setConfig(normalizedConfig);
    setValidationErrors(collectValidationErrors(normalizedConfig));
  };

  if (loading) {
    return (
      <div class="empty-state">
        <div class="text-center">
          <h2>Loading Settings...</h2>
        </div>
      </div>
    );
  }

  if (!config) {
    return (
      <div class="empty-state tone-red">
        Failed to load configuration
      </div>
    );
  }

  return (
    <div class="panel-page settings-page page-enter">
      {message && (
        <div
          ref={errorPanelRef}
          class={`settings-message ${message.type === 'success' ? 'settings-message-success' : 'settings-message-error'}`}
        >
          <p>{message.text}</p>
        </div>
      )}

      {/* Device Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Device</h2>
        <div class="space-y-4">
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Device Name
            </label>
            <input
              data-field="deviceName"
              type="text"
              value={config.deviceName}
              onChange={(e) => updateConfig(['deviceName'], (e.target as HTMLInputElement).value)}
              class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                errorFor('deviceName') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
              }`}
            />
            {errorFor('deviceName') && (
              <p class="mt-1 text-sm text-red-400">{errorFor('deviceName')}</p>
            )}
          </div>
        </div>
      </section>

      {/* WiFi Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">WiFi</h2>
        <div class="space-y-4">
          <div>
            <div class="flex items-center justify-between mb-2">
              <label class="block text-sm font-medium text-gray-300">
                Network
              </label>
              <button
                type="button"
                onClick={scanWifiNetworks}
                disabled={scanningWifi}
                class="text-sm px-3 py-1 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 rounded text-white transition-colors"
              >
                {scanningWifi ? 'Scanning...' : 'Scan Networks'}
              </button>
            </div>
            <select
              data-field="wifi.ssid"
              value={config.wifi.ssid === '' ? 'OTHER' : 
                     (wifiNetworks || []).some(n => n.ssid === config.wifi.ssid) ? config.wifi.ssid : 
                     config.wifi.ssid}
              onChange={(e) => {
                const value = (e.target as HTMLSelectElement).value;
                if (value === 'OTHER') {
                  updateWifiNetwork('');
                } else {
                  updateWifiNetwork(value);
                }
              }}
              class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                errorFor('wifi.ssid') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
              }`}
            >
              {/* Current network if not in scan results and not empty */}
              {config.wifi.ssid && !(wifiNetworks || []).some(n => n.ssid === config.wifi.ssid) && (
                <option value={config.wifi.ssid}>{config.wifi.ssid} (current)</option>
              )}
              
              {/* Scanned networks */}
              {(wifiNetworks || []).map((network) => (
                <option key={network.ssid} value={network.ssid}>
                  {network.ssid} ({network.rssi} dBm) {network.encryption !== 'Open' ? '🔒' : ''}
                </option>
              ))}
              
              {/* Other option always last */}
              <option value="OTHER">Other (manual entry)...</option>
            </select>
            
            {/* Only show manual input when "Other" is selected */}
            {config.wifi.ssid === '' && (
              <input
                data-field="wifi.ssid"
                type="text"
                value={config.wifi.ssid}
                onChange={(e) => updateWifiNetwork((e.target as HTMLInputElement).value)}
                placeholder="Enter network name (SSID)"
                class="mt-2 w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                autoFocus
              />
            )}
            {errorFor('wifi.ssid') && (
              <p class="mt-1 text-sm text-red-400">{errorFor('wifi.ssid')}</p>
            )}
          </div>
          {shouldShowWifiPassword && (
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                Password
              </label>
              <input
                data-field="wifi.password"
                type="password"
                value={config.wifi.password}
                onChange={(e) => updateConfig(['wifi', 'password'], (e.target as HTMLInputElement).value)}
                placeholder="Network password"
                class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
              />
            </div>
          )}
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Hostname
            </label>
            <input
              data-field="wifi.hostname"
              type="text"
              value={config.wifi.hostname}
              onChange={(e) => updateConfig(['wifi', 'hostname'], (e.target as HTMLInputElement).value)}
              class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                errorFor('wifi.hostname') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
              }`}
            />
            {errorFor('wifi.hostname') && (
              <p class="mt-1 text-sm text-red-400">{errorFor('wifi.hostname')}</p>
            )}
          </div>
          <div class="flex items-center">
            <input
              data-field="wifi.autoReconnect"
              type="checkbox"
              checked={config.wifi.autoReconnect}
              onChange={(e) => updateConfig(['wifi', 'autoReconnect'], (e.target as HTMLInputElement).checked)}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Auto Reconnect
            </label>
          </div>
        </div>
      </section>

      {/* OTA Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Over-The-Air Updates</h2>
        <div class="space-y-4">
          <div class="flex items-center">
            <input
              data-field="ntp.enabled"
              type="checkbox"
              checked={config.ota.enabled}
              onChange={(e) => updateConfig(['ota', 'enabled'], (e.target as HTMLInputElement).checked)}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Enable command-line OTA
            </label>
          </div>
          {config.ota.enabled && (
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                Password
              </label>
              <input
                data-field="ota.password"
                type="password"
                value={config.ota.password}
                onChange={(e) => updateConfig(['ota', 'password'], (e.target as HTMLInputElement).value)}
                class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                  validationErrors['ota.password'] || validationErrors.otaPassword ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                }`}
              />
              {(validationErrors['ota.password'] || validationErrors.otaPassword) && (
                <p class="mt-1 text-sm text-red-400">{validationErrors['ota.password'] || validationErrors.otaPassword}</p>
              )}
            </div>
          )}
        </div>
      </section>

      {/* HTTP Auth Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">HTTP Authentication</h2>
        <div class="space-y-4">
          <div class="flex items-center">
            <input
              type="checkbox"
              checked={config.auth?.enabled ?? false}
              onChange={(e) => updateConfig(['auth', 'enabled'], (e.target as HTMLInputElement).checked)}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Require authentication for config and OTA endpoints
            </label>
          </div>
          {config.auth?.enabled && (
            <>
              <div class="p-3 bg-yellow-900 border border-yellow-700 rounded-lg text-sm text-yellow-200">
                When enabled, saving config, OTA updates, and restart require a username and password.
                Read-only sensor and status endpoints remain accessible without credentials.
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Username
                </label>
                <input
                  data-field="auth.username"
                  type="text"
                  value={config.auth.username}
                  onChange={(e) => updateConfig(['auth', 'username'], (e.target as HTMLInputElement).value)}
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors['auth.username'] ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors['auth.username'] && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors['auth.username']}</p>
                )}
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Password
                </label>
                <input
                  data-field="auth.password"
                  type="password"
                  value={config.auth.password}
                  onChange={(e) => updateConfig(['auth', 'password'], (e.target as HTMLInputElement).value)}
                  placeholder="Leave empty to clear; send placeholder to preserve stored value"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors['auth.password'] ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors['auth.password'] && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors['auth.password']}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">
                  Stored password is masked. Type a new password to change it, or leave the masked value to keep the current one.
                </p>
              </div>
            </>
          )}
        </div>
      </section>

      {/* NTP & Time Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Time & NTP</h2>
        <div class="space-y-4">
          <div class="flex items-center">
            <input
              type="checkbox"
              checked={config.ntp.enabled}
              onChange={(e) => {
                const newValue = (e.target as HTMLInputElement).checked;
                // Don't allow disabling if GPS is also disabled
                if (!newValue && !config.gps.enabled) {
                  alert('At least one time source (NTP or GPS) must be enabled');
                  return;
                }
                updateConfig(['ntp', 'enabled'], newValue);
              }}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Enable NTP Time Sync
            </label>
          </div>
          {errorFor('ntp.enabled') && (
            <p class="text-sm text-red-400">{errorFor('ntp.enabled')}</p>
          )}
          
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Timezone
            </label>
            <select
              data-field="ntp.timezone"
              value={
                TIMEZONE_OPTIONS.some(opt => opt.value === config.ntp.timezone) 
                  ? config.ntp.timezone 
                  : 'custom'
              }
              onChange={(e) => {
                const value = (e.target as HTMLSelectElement).value;
                if (value !== 'custom') {
                  updateConfig(['ntp', 'timezone'], value);
                }
              }}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            >
              {TIMEZONE_OPTIONS.map(tz => (
                <option key={tz.value} value={tz.value}>{tz.label}</option>
              ))}
            </select>
          </div>

          {(!TIMEZONE_OPTIONS.some(opt => opt.value === config.ntp.timezone) || 
            TIMEZONE_OPTIONS.find(opt => opt.value === config.ntp.timezone)?.value === 'custom') && (
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                Custom Timezone (POSIX format)
              </label>
              <input
                data-field="ntp.timezone"
                type="text"
                value={config.ntp.timezone}
                onChange={(e) => updateConfig(['ntp', 'timezone'], (e.target as HTMLInputElement).value)}
                placeholder="e.g., PST8PDT,M3.2.0,M11.1.0"
                class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
              />
              <p class="mt-1 text-xs text-gray-500">
                Format: STDoffset[DST[offset][,start[/time],end[/time]]]
              </p>
            </div>
          )}

          {config.ntp.enabled && (
            <>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Primary NTP Server
                </label>
                <input
                  data-field="ntp.server1"
                  type="text"
                  value={config.ntp.server1}
                  onChange={(e) => updateConfig(['ntp', 'server1'], (e.target as HTMLInputElement).value)}
                  placeholder="pool.ntp.org"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('ntp.server1') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('ntp.server1') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('ntp.server1')}</p>
                )}
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Secondary NTP Server
                </label>
                <input
                  data-field="ntp.server2"
                  type="text"
                  value={config.ntp.server2}
                  onChange={(e) => updateConfig(['ntp', 'server2'], (e.target as HTMLInputElement).value)}
                  placeholder="time.nist.gov"
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Sync Interval (minutes)
                </label>
                <input
                  data-field="ntp.syncIntervalMs"
                  type="number"
                  value={config.ntp.syncIntervalMs / 60000}
                  onChange={(e) => updateConfig(['ntp', 'syncIntervalMs'], parseInt((e.target as HTMLInputElement).value) * 60000)}
                  min="10"
                  max="1440"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('ntp.syncIntervalMs') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('ntp.syncIntervalMs') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('ntp.syncIntervalMs')}</p>
                )}
              </div>
            </>
          )}
        </div>
      </section>

      {/* GPS Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">GPS</h2>
        <div class="space-y-4">
          <div class="flex items-center">
            <input
              data-field="gps.enabled"
              type="checkbox"
              checked={config.gps.enabled}
              onChange={(e) => {
                const newValue = (e.target as HTMLInputElement).checked;
                // Don't allow disabling if NTP is also disabled
                if (!newValue && !config.ntp.enabled) {
                  alert('At least one time source (NTP or GPS) must be enabled');
                  return;
                }
                updateConfig(['gps', 'enabled'], newValue);
              }}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Enable GPS
            </label>
          </div>

          {config.gps.enabled && (
            <>
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    RX Pin
                  </label>
                  <input
                    data-field="gps.rxPin"
                    type="number"
                    value={config.gps.rxPin}
                    onChange={(e) => updateConfig(['gps', 'rxPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('gps.rxPin') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('gps.rxPin') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('gps.rxPin')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    TX Pin
                  </label>
                  <input
                    data-field="gps.txPin"
                    type="number"
                    value={config.gps.txPin}
                    onChange={(e) => updateConfig(['gps', 'txPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('gps.txPin') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('gps.txPin') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('gps.txPin')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Baud Rate
                  </label>
                  <select
                    data-field="gps.baudRate"
                    value={config.gps.baudRate}
                    onChange={(e) => updateConfig(['gps', 'baudRate'], parseInt((e.target as HTMLSelectElement).value))}
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  >
                    <option value="4800">4800</option>
                    <option value="9600">9600</option>
                    <option value="19200">19200</option>
                    <option value="38400">38400</option>
                    <option value="57600">57600</option>
                    <option value="115200">115200</option>
                  </select>
                  {errorFor('gps.baudRate') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('gps.baudRate')}</p>
                  )}
                </div>
              </div>
            </>
          )}

          {/* Time Source Priority */}
          <div class="mt-6 pt-6 border-t border-gray-700">
            <h3 class="text-lg font-semibold text-white mb-4">Time Synchronization Priority</h3>
            <p class="text-sm text-gray-400 mb-4">
              Configure which time source to try first. If the primary source fails, the system will automatically fall back to the secondary source.
            </p>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Primary Source
                </label>
                <select
                  data-field="primaryTimeSource"
                  value={config.primaryTimeSource}
                  onChange={(e) => {
                    const newPrimary = parseInt((e.target as HTMLSelectElement).value);
                    updateConfig(['primaryTimeSource'], newPrimary);
                    // Auto-adjust secondary if they're the same
                    if (newPrimary === config.secondaryTimeSource) {
                      const otherSource = newPrimary === 0 ? 1 : 0;
                      if ((otherSource === 0 && config.ntp.enabled) || (otherSource === 1 && config.gps.enabled)) {
                        updateConfig(['secondaryTimeSource'], otherSource);
                      }
                    }
                  }}
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                >
                  {config.ntp.enabled && <option value="0">NTP</option>}
                  {config.gps.enabled && <option value="1">GPS</option>}
                </select>
                {errorFor('primaryTimeSource') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('primaryTimeSource')}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">
                  Try this source first
                </p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Secondary Source (Fallback)
                </label>
                <select
                  data-field="secondaryTimeSource"
                  value={config.secondaryTimeSource}
                  onChange={(e) => updateConfig(['secondaryTimeSource'], parseInt((e.target as HTMLSelectElement).value))}
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                >
                  {config.ntp.enabled && config.primaryTimeSource !== 0 && <option value="0">NTP</option>}
                  {config.gps.enabled && config.primaryTimeSource !== 1 && <option value="1">GPS</option>}
                </select>
                {errorFor('secondaryTimeSource') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('secondaryTimeSource')}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">
                  Use if primary source is unavailable
                </p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* MQTT Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">MQTT</h2>
        <div class="space-y-4">
          <div class="flex items-center">
            <input
              type="checkbox"
              checked={config.mqtt.enabled}
              onChange={(e) => updateConfig(['mqtt', 'enabled'], (e.target as HTMLInputElement).checked)}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Enable MQTT
            </label>
          </div>
          {config.mqtt.enabled && (
            <>
              <div>
                <label data-required class="block text-sm font-medium text-gray-300 mb-2">
                  Broker
                </label>
                <input
                  data-field="mqttBroker"
                  type="text"
                  value={config.mqtt.broker}
                  onChange={(e) => updateConfig(['mqtt', 'broker'], (e.target as HTMLInputElement).value)}
                  placeholder="mqtt.example.com or 192.168.1.100"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('mqttBroker') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('mqttBroker') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('mqttBroker')}</p>
                )}
              </div>
              <div>
                <label data-required class="block text-sm font-medium text-gray-300 mb-2">
                  Port
                </label>
                <input
                  data-field="mqttPort"
                  type="number"
                  value={config.mqtt.port}
                  onChange={(e) => updateConfig(['mqtt', 'port'], Math.max(1, Math.min(65535, parseInt((e.target as HTMLInputElement).value) || 1883)))}
                  min="1"
                  max="65535"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('mqttPort') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('mqttPort') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('mqttPort')}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">Default: 1883 (unencrypted), 8883 (TLS)</p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Username
                </label>
                <input
                  type="text"
                  value={config.mqtt.username}
                  onChange={(e) => updateConfig(['mqtt', 'username'], (e.target as HTMLInputElement).value)}
                  placeholder="Leave empty if no authentication required"
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Password
                </label>
                <input
                  type="password"
                  value={config.mqtt.password}
                  onChange={(e) => updateConfig(['mqtt', 'password'], (e.target as HTMLInputElement).value)}
                  placeholder="Leave empty if no authentication required"
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
              </div>
              <div>
                <label data-required class="block text-sm font-medium text-gray-300 mb-2">
                  Topic
                </label>
                <input
                  data-field="mqttTopic"
                  type="text"
                  value={config.mqtt.topic}
                  onChange={(e) => updateConfig(['mqtt', 'topic'], (e.target as HTMLInputElement).value)}
                  placeholder="sqm/data"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('mqttTopic') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('mqttTopic') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('mqttTopic')}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">Valid characters: a-z, A-Z, 0-9, /, _, -</p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Publish Interval (seconds)
                </label>
                <input
                  data-field="mqttInterval"
                  type="number"
                  value={config.mqtt.publishIntervalMs / 1000}
                  onChange={(e) => updateConfig(['mqtt', 'publishIntervalMs'], Math.max(1, parseInt((e.target as HTMLInputElement).value) || 60) * 1000)}
                  min="1"
                  max="3600"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    errorFor('mqttInterval') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {errorFor('mqttInterval') && (
                  <p class="mt-1 text-sm text-red-400">{errorFor('mqttInterval')}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">How often to publish sensor data (1-3600 seconds)</p>
              </div>

              {/* MQTT Test Connection */}
              <div class="pt-4 border-t border-gray-700">
                <button
                  type="button"
                  onClick={testMqttConnection}
                  disabled={testingMqtt || !config.mqtt.broker || !config.mqtt.port}
                  class="w-full px-4 py-2 bg-purple-600 hover:bg-purple-700 disabled:bg-gray-600 text-white font-semibold rounded-lg transition-colors"
                >
                  {testingMqtt ? 'Testing Connection...' : 'Test MQTT Connection'}
                </button>
                {mqttTestResult && (
                  <div class={`mt-3 p-3 rounded-lg ${
                    mqttTestResult.type === 'success' ? 'bg-green-900 border border-green-700' : 'bg-red-900 border border-red-700'
                  }`}>
                    <p class="text-white text-sm">
                      {mqttTestResult.text}
                    </p>
                  </div>
                )}
              </div>
            </>
          )}
        </div>
      </section>

      {/* Sensor Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Sensors</h2>
        <div class="space-y-4">
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Read Interval (ms)
            </label>
            <input
              data-field="sensorInterval"
              type="number"
              value={config.sensor.readIntervalMs}
              onChange={(e) => updateConfig(['sensor', 'readIntervalMs'], Math.max(100, parseInt((e.target as HTMLInputElement).value) || 5000))}
              min="100"
              max="3600000"
              step="100"
              class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                errorFor('sensorInterval') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
              }`}
            />
            {errorFor('sensorInterval') && (
              <p class="mt-1 text-sm text-red-400">{errorFor('sensorInterval')}</p>
            )}
            <p class="mt-1 text-xs text-gray-500">Minimum: 100ms, Maximum: 1 hour (3600000ms)</p>
          </div>
          <div class="grid grid-cols-2 gap-4">
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                I2C SDA Pin
              </label>
              <input
                data-field="i2cSDA"
                type="number"
                value={config.sensor.i2cSDA}
                onChange={(e) => updateConfig(['sensor', 'i2cSDA'], parseInt((e.target as HTMLInputElement).value) || 21)}
                class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                  errorFor('i2cSDA') || errorFor('i2cPins') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                }`}
              />
              {errorFor('i2cSDA') && (
                <p class="mt-1 text-sm text-red-400">{errorFor('i2cSDA')}</p>
              )}
            </div>
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                I2C SCL Pin
              </label>
              <input
                data-field="i2cSCL"
                type="number"
                value={config.sensor.i2cSCL}
                onChange={(e) => updateConfig(['sensor', 'i2cSCL'], parseInt((e.target as HTMLInputElement).value) || 22)}
                class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                  errorFor('i2cSCL') || errorFor('i2cPins') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                }`}
              />
              {errorFor('i2cSCL') && (
                <p class="mt-1 text-sm text-red-400">{errorFor('i2cSCL')}</p>
              )}
            </div>
          </div>
          {errorFor('i2cPins') && (
            <p class="text-sm text-red-400">{errorFor('i2cPins')}</p>
          )}
          <p class="text-xs text-gray-500">
            Common: SDA=21, SCL=22. Valid GPIOs: 0,1,2,3,4,5,12-19,21-23,25-27,32-36,39
          </p>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              I2C Frequency (Hz)
            </label>
            <select
              data-field="i2cFrequency"
              value={config.sensor.i2cFrequency}
              onChange={(e) => updateConfig(['sensor', 'i2cFrequency'], parseInt((e.target as HTMLSelectElement).value))}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            >
              <option value="10000">10 kHz (Slow)</option>
              <option value="50000">50 kHz</option>
              <option value="100000">100 kHz (Standard)</option>
              <option value="400000">400 kHz (Fast)</option>
            </select>
            {errorFor('i2cFrequency') && (
              <p class="mt-1 text-sm text-red-400">{errorFor('i2cFrequency')}</p>
            )}
          </div>
        </div>
      </section>

      {/* Cloud Detection Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Cloud Detection</h2>
        <p class="text-sm text-gray-400 mb-4">
          These thresholds control when the IR temperature sensor classifies the sky as clear, cloudy, or overcast. Lower (more negative) clear sky threshold = more strict clear-sky classification.
        </p>
        <div class="space-y-4">
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Clear Sky Threshold (°C)
            </label>
            <input
              type="number"
              value={config.cloudDetection.clearSkyThreshold}
              onChange={(e) => updateConfig(['cloudDetection', 'clearSkyThreshold'], parseFloat((e.target as HTMLInputElement).value))}
              min="-30"
              max="0"
              step="0.1"
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
            <p class="mt-1 text-xs text-gray-500">Corrected delta below which sky is classified as clear (default: -13.0°C)</p>
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Cloudy Threshold (°C)
            </label>
            <input
              type="number"
              value={config.cloudDetection.cloudyThreshold}
              onChange={(e) => updateConfig(['cloudDetection', 'cloudyThreshold'], parseFloat((e.target as HTMLInputElement).value))}
              min="-20"
              max="10"
              step="0.1"
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
            <p class="mt-1 text-xs text-gray-500">Corrected delta above which sky is classified as overcast (default: -3.0°C)</p>
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Humidity Correction Factor
            </label>
            <input
              type="number"
              value={config.cloudDetection.humidityCorrection}
              onChange={(e) => updateConfig(['cloudDetection', 'humidityCorrection'], parseFloat((e.target as HTMLInputElement).value))}
              min="0"
              max="2"
              step="0.01"
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
            <p class="mt-1 text-xs text-gray-500">k1 factor for AAG CloudWatcher humidity correction formula (default: 0.75)</p>
          </div>
        </div>
      </section>

      {/* Rain Sensor Settings */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Rain Sensor</h2>
        <div class="space-y-4">
          <div class="p-3 bg-yellow-900 border border-yellow-700 rounded-lg text-sm text-yellow-200">
            Initialised only means the UART was opened. Online requires a valid response from the RG-15.
          </div>
          <div class="flex items-center">
            <input
              type="checkbox"
              checked={config.rain?.enabled ?? false}
              onChange={(e) => updateConfig(['rain', 'enabled'], (e.target as HTMLInputElement).checked)}
              class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
            />
            <label class="ml-2 text-sm font-medium text-gray-300">
              Enable RG-15 Rain Sensor
            </label>
          </div>

          {config.rain?.enabled && (
            <>
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    RX Pin
                  </label>
                  <input
                    data-field="rain.rxPin"
                    type="number"
                    value={config.rain.rxPin}
                    onChange={(e) => updateConfig(['rain', 'rxPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('rain.rxPin') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('rain.rxPin') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('rain.rxPin')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    TX Pin
                  </label>
                  <input
                    data-field="rain.txPin"
                    type="number"
                    value={config.rain.txPin}
                    onChange={(e) => updateConfig(['rain', 'txPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('rain.txPin') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('rain.txPin') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('rain.txPin')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Baud Rate
                  </label>
                  <select
                    data-field="rain.baudRate"
                    value={config.rain.baudRate}
                    onChange={(e) => updateConfig(['rain', 'baudRate'], parseInt((e.target as HTMLSelectElement).value))}
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  >
                    <option value="2400">2400</option>
                    <option value="4800">4800</option>
                    <option value="9600">9600</option>
                    <option value="19200">19200</option>
                  </select>
                  {errorFor('rain.baudRate') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('rain.baudRate')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Poll interval (seconds)
                  </label>
                  <input
                    data-field="rain.pollIntervalMs"
                    type="number"
                    min="1"
                    max="3600"
                    value={Math.round((config.rain.pollIntervalMs ?? 5000) / 1000)}
                    onChange={(e) => updateConfig(['rain', 'pollIntervalMs'], Math.max(1, parseInt((e.target as HTMLInputElement).value) || 5) * 1000)}
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('rain.pollIntervalMs') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('rain.pollIntervalMs') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('rain.pollIntervalMs')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Raining clear delay (minutes)
                  </label>
                  <input
                    data-field="rain.rainClearDelayMs"
                    type="number"
                    min="1"
                    max="1440"
                    value={Math.round((config.rain.rainClearDelayMs ?? 900000) / 60000)}
                    onChange={(e) => updateConfig(['rain', 'rainClearDelayMs'], Math.max(1, parseInt((e.target as HTMLInputElement).value) || 15) * 60000)}
                    class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                      errorFor('rain.rainClearDelayMs') ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                    }`}
                  />
                  {errorFor('rain.rainClearDelayMs') && (
                    <p class="mt-1 text-sm text-red-400">{errorFor('rain.rainClearDelayMs')}</p>
                  )}
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Resolution
                  </label>
                  <select
                    value={config.rain.resolution ?? 'switch'}
                    onChange={(e) => updateConfig(['rain', 'resolution'], (e.target as HTMLSelectElement).value)}
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  >
                    <option value="high">High (0.01 mm)</option>
                    <option value="low">Low (0.2 mm)</option>
                    <option value="switch">Switch (by jumper)</option>
                  </select>
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Units
                  </label>
                  <select
                    value={config.rain.units ?? 'metric'}
                    onChange={(e) => updateConfig(['rain', 'units'], (e.target as HTMLSelectElement).value)}
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  >
                    <option value="metric">Metric (mm)</option>
                    <option value="imperial">Imperial (inches)</option>
                    <option value="switch">Switch (by jumper)</option>
                  </select>
                </div>
                <div class="md:col-span-3 flex items-center">
                  <input
                    type="checkbox"
                    checked={config.rain.debugUart}
                    onChange={(e) => updateConfig(['rain', 'debugUart'], (e.target as HTMLInputElement).checked)}
                    class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                  />
                  <label class="ml-2 text-sm font-medium text-gray-300">
                    Enable RG-15 UART debug logging
                  </label>
                </div>
                <div class="md:col-span-3 flex items-center">
                  <input
                    type="checkbox"
                    checked={config.rain.dailyResetEnabled ?? false}
                    onChange={(e) => updateConfig(['rain', 'dailyResetEnabled'], (e.target as HTMLInputElement).checked)}
                    class="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                  />
                  <label class="ml-2 text-sm font-medium text-gray-300">
                    Reset RG-15 total accumulation daily
                  </label>
                </div>
                {config.rain.dailyResetEnabled && (
                  <div>
                    <label class="block text-sm font-medium text-gray-300 mb-2">Daily reset time</label>
                    <input
                      data-field="rain.dailyResetHour"
                      type="time"
                      value={`${String(config.rain.dailyResetHour ?? 0).padStart(2, '0')}:${String(config.rain.dailyResetMinute ?? 0).padStart(2, '0')}`}
                      onChange={(e) => updateDailyResetTime((e.target as HTMLInputElement).value)}
                      class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                    />
                    {(errorFor('rain.dailyResetHour') || errorFor('rain.dailyResetMinute')) && (
                      <p class="mt-1 text-sm text-red-400">{errorFor('rain.dailyResetHour') || errorFor('rain.dailyResetMinute')}</p>
                    )}
                  </div>
                )}
              </div>

              <div class="pt-4 border-t border-gray-700 space-y-3">
                <button
                  type="button"
                  onClick={testRg15Communication}
                  disabled={testingRain}
                  class="w-full px-4 py-2 bg-cyan-600 hover:bg-cyan-700 disabled:bg-gray-600 text-white font-semibold rounded-lg transition-colors"
                >
                  {testingRain ? 'Testing RG-15...' : 'Test RG-15 communication'}
                </button>
                <p class="text-xs text-gray-500">
                  This sends a harmless read command and reports the raw response, acknowledgement, and parse status.
                </p>
                {rainTestResult && (
                  <div class={`p-3 rounded-lg ${
                    rainTestResult.type === 'success' ? 'bg-green-900 border border-green-700' : 'bg-red-900 border border-red-700'
                  }`}>
                    <p class="text-white text-sm">
                      {rainTestResult.text}
                    </p>
                  </div>
                )}
              </div>
            </>
          )}
        </div>
      </section>

      {/* Save Button */}
      <div class="settings-save-bar">
        <button
          onClick={saveConfig}
          disabled={saving}
          class="px-6 py-3 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 text-white font-semibold rounded-lg transition-colors"
        >
          {saving ? 'Saving...' : 'Save Configuration'}
        </button>
      </div>
    </div>
  );
};

export default Settings;
