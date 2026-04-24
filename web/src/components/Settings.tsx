import { FunctionalComponent } from 'preact';
import { useState, useEffect, useRef } from 'preact/hooks';
import type { Config, WiFiNetwork } from '../types';
import { configSchema } from '../validation/configSchema';
import { ZodError } from 'zod';

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

const Settings: FunctionalComponent = () => {
  const [config, setConfig] = useState<Config | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [wifiNetworks, setWifiNetworks] = useState<WiFiNetwork[]>([]);
  const [scanningWifi, setScanningWifi] = useState(false);
  const [validationErrors, setValidationErrors] = useState<Record<string, string>>({});
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttTestResult, setMqttTestResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const errorPanelRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const response = await fetch('/api/config');
      const data = await response.json();
      setConfig(data);
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

  const validateConfig = (): boolean => {
    if (!config) return false;
    
    try {
      configSchema.parse(config);
      setValidationErrors({});
      console.log('✅ Validation passed');
      return true;
    } catch (error) {
      if (error instanceof ZodError) {
        const errors: Record<string, string> = {};
        console.error('❌ Validation failed:', error.issues);
        
        error.issues.forEach((issue) => {
          // Map nested paths to flat field names used in the UI
          const path = issue.path.join('.');
          const flatPath = path.replace(/\./g, '');
          
          // Store both versions for flexibility
          errors[path] = issue.message;
          errors[flatPath] = issue.message;
          
          console.log(`  - ${path}: ${issue.message}`);
        });
        
        setValidationErrors(errors);
        
        // Scroll to error panel
        setTimeout(() => {
          errorPanelRef.current?.scrollIntoView({ behavior: 'smooth', block: 'center' });
        }, 100);
        console.log('Validation errors set:', errors);
      }
      return false;
    }
  };

  const saveConfig = async () => {
    if (!config) return;

    console.log('💾 Attempting to save config:', config);

    if (!validateConfig()) {
      const errorCount = Object.keys(validationErrors).length / 2; // Divide by 2 since we store both versions
      setMessage({ type: 'error', text: `Please fix ${errorCount} validation error(s) - check console for details` });
      return;
    }

    setSaving(true);
    setMessage(null);

    try {
      const response = await fetch('/api/config', {
        method: 'PUT',  // Using PUT for full resource replacement
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config),
      });

      if (response.ok) {
        setMessage({ type: 'success', text: 'Configuration saved successfully!' });
        setValidationErrors({});
      } else {
        const error = await response.json();
        setMessage({ type: 'error', text: error.error || 'Failed to save configuration' });
      }
    } catch (error) {
      setMessage({ type: 'error', text: 'Network error occurred' });
    } finally {
      setSaving(false);
    }
  };

  const updateConfig = (path: string[], value: any) => {
    if (!config) return;

    const newConfig = { ...config };
    let current: any = newConfig;
    
    for (let i = 0; i < path.length - 1; i++) {
      current = current[path[i]];
    }
    
    current[path[path.length - 1]] = value;
    setConfig(newConfig);
  };

  if (loading) {
    return (
      <div class="flex items-center justify-center min-h-[60vh]">
        <div class="text-center">
          <div class="text-6xl mb-4">⚙️</div>
          <h2 class="text-2xl font-bold text-white">Loading Settings...</h2>
        </div>
      </div>
    );
  }

  if (!config) {
    return (
      <div class="text-center text-red-400">
        Failed to load configuration
      </div>
    );
  }

  return (
    <div class="max-w-4xl mx-auto space-y-6">
      <h1 class="text-3xl font-bold text-white mb-6">Settings</h1>

      {message && (
        <div class={`p-4 rounded-lg ${
          message.type === 'success' ? 'bg-green-900 border-green-700' : 'bg-red-900 border-red-700'
        } border`}>
          <p class="text-white">{message.text}</p>
        </div>
      )}

      {/* Validation Errors Debug Panel */}
      {Object.keys(validationErrors).length > 0 && (
        <div ref={errorPanelRef} class="bg-orange-900 border border-orange-700 rounded-lg p-4">
          <h3 class="text-lg font-semibold text-white mb-2">⚠️ Validation Errors</h3>
          <ul class="space-y-1 text-sm text-orange-200">
            {Object.entries(validationErrors)
              .filter(([key]) => !key.includes('.')) // Only show the flat paths
              .map(([field, error]) => (
                <li key={field}>
                  <strong>{field}:</strong> {error}
                </li>
              ))}
          </ul>
          <p class="text-xs text-orange-300 mt-2">Check browser console for detailed path information</p>
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
              type="text"
              value={config.deviceName}
              onChange={(e) => updateConfig(['deviceName'], (e.target as HTMLInputElement).value)}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
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
                {scanningWifi ? 'Scanning...' : '📡 Scan Networks'}
              </button>
            </div>
            <select
              value={config.wifi.ssid === '' ? 'OTHER' : 
                     (wifiNetworks || []).some(n => n.ssid === config.wifi.ssid) ? config.wifi.ssid : 
                     config.wifi.ssid}
              onChange={(e) => {
                const value = (e.target as HTMLSelectElement).value;
                if (value === 'OTHER') {
                  updateConfig(['wifi', 'ssid'], '');
                } else {
                  updateConfig(['wifi', 'ssid'], value);
                }
              }}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
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
                type="text"
                value={config.wifi.ssid}
                onChange={(e) => updateConfig(['wifi', 'ssid'], (e.target as HTMLInputElement).value)}
                placeholder="Enter network name (SSID)"
                class="mt-2 w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                autoFocus
              />
            )}
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Password
            </label>
            <input
              type="password"
              value={config.wifi.password}
              onChange={(e) => updateConfig(['wifi', 'password'], (e.target as HTMLInputElement).value)}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Hostname
            </label>
            <input
              type="text"
              value={config.wifi.hostname}
              onChange={(e) => updateConfig(['wifi', 'hostname'], (e.target as HTMLInputElement).value)}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            />
          </div>
          <div class="flex items-center">
            <input
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
          
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Timezone
            </label>
            <select
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
                  type="text"
                  value={config.ntp.server1}
                  onChange={(e) => updateConfig(['ntp', 'server1'], (e.target as HTMLInputElement).value)}
                  placeholder="pool.ntp.org"
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Secondary NTP Server (optional)
                </label>
                <input
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
                  type="number"
                  value={config.ntp.syncIntervalMs / 60000}
                  onChange={(e) => updateConfig(['ntp', 'syncIntervalMs'], parseInt((e.target as HTMLInputElement).value) * 60000)}
                  min="10"
                  max="1440"
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
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
                    type="number"
                    value={config.gps.rxPin}
                    onChange={(e) => updateConfig(['gps', 'rxPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  />
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    TX Pin
                  </label>
                  <input
                    type="number"
                    value={config.gps.txPin}
                    onChange={(e) => updateConfig(['gps', 'txPin'], parseInt((e.target as HTMLInputElement).value))}
                    min="0"
                    max="39"
                    class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                  />
                </div>
                <div>
                  <label class="block text-sm font-medium text-gray-300 mb-2">
                    Baud Rate
                  </label>
                  <select
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
                <p class="mt-1 text-xs text-gray-500">
                  Try this source first
                </p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Secondary Source (Fallback)
                </label>
                <select
                  value={config.secondaryTimeSource}
                  onChange={(e) => updateConfig(['secondaryTimeSource'], parseInt((e.target as HTMLSelectElement).value))}
                  class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
                >
                  {config.ntp.enabled && config.primaryTimeSource !== 0 && <option value="0">NTP</option>}
                  {config.gps.enabled && config.primaryTimeSource !== 1 && <option value="1">GPS</option>}
                </select>
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
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Broker *
                </label>
                <input
                  type="text"
                  value={config.mqtt.broker}
                  onChange={(e) => updateConfig(['mqtt', 'broker'], (e.target as HTMLInputElement).value)}
                  placeholder="mqtt.example.com or 192.168.1.100"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors.mqttBroker ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors.mqttBroker && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors.mqttBroker}</p>
                )}
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Port *
                </label>
                <input
                  type="number"
                  value={config.mqtt.port}
                  onChange={(e) => updateConfig(['mqtt', 'port'], Math.max(1, Math.min(65535, parseInt((e.target as HTMLInputElement).value) || 1883)))}
                  min="1"
                  max="65535"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors.mqttPort ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors.mqttPort && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors.mqttPort}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">Default: 1883 (unencrypted), 8883 (TLS)</p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Username (optional)
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
                  Password (optional)
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
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Topic *
                </label>
                <input
                  type="text"
                  value={config.mqtt.topic}
                  onChange={(e) => updateConfig(['mqtt', 'topic'], (e.target as HTMLInputElement).value)}
                  placeholder="sqm/data"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors.mqttTopic ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors.mqttTopic && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors.mqttTopic}</p>
                )}
                <p class="mt-1 text-xs text-gray-500">Valid characters: a-z, A-Z, 0-9, /, _, -</p>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-300 mb-2">
                  Publish Interval (seconds)
                </label>
                <input
                  type="number"
                  value={config.mqtt.publishIntervalMs / 1000}
                  onChange={(e) => updateConfig(['mqtt', 'publishIntervalMs'], Math.max(1, parseInt((e.target as HTMLInputElement).value) || 60) * 1000)}
                  min="1"
                  max="3600"
                  class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                    validationErrors.mqttInterval ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                  }`}
                />
                {validationErrors.mqttInterval && (
                  <p class="mt-1 text-sm text-red-400">{validationErrors.mqttInterval}</p>
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
                  {testingMqtt ? '🔄 Testing Connection...' : '🔌 Test MQTT Connection'}
                </button>
                {mqttTestResult && (
                  <div class={`mt-3 p-3 rounded-lg ${
                    mqttTestResult.type === 'success' ? 'bg-green-900 border border-green-700' : 'bg-red-900 border border-red-700'
                  }`}>
                    <p class="text-white text-sm">
                      {mqttTestResult.type === 'success' ? '✅ ' : '❌ '}
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
              type="number"
              value={config.sensor.readIntervalMs}
              onChange={(e) => updateConfig(['sensor', 'readIntervalMs'], Math.max(100, parseInt((e.target as HTMLInputElement).value) || 5000))}
              min="100"
              max="3600000"
              step="100"
              class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                validationErrors.sensorInterval ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
              }`}
            />
            {validationErrors.sensorInterval && (
              <p class="mt-1 text-sm text-red-400">{validationErrors.sensorInterval}</p>
            )}
            <p class="mt-1 text-xs text-gray-500">Minimum: 100ms, Maximum: 1 hour (3600000ms)</p>
          </div>
          <div class="grid grid-cols-2 gap-4">
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                I2C SDA Pin
              </label>
              <input
                type="number"
                value={config.sensor.i2cSDA}
                onChange={(e) => updateConfig(['sensor', 'i2cSDA'], parseInt((e.target as HTMLInputElement).value) || 21)}
                class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                  validationErrors.i2cSDA || validationErrors.i2cPins ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                }`}
              />
              {validationErrors.i2cSDA && (
                <p class="mt-1 text-sm text-red-400">{validationErrors.i2cSDA}</p>
              )}
            </div>
            <div>
              <label class="block text-sm font-medium text-gray-300 mb-2">
                I2C SCL Pin
              </label>
              <input
                type="number"
                value={config.sensor.i2cSCL}
                onChange={(e) => updateConfig(['sensor', 'i2cSCL'], parseInt((e.target as HTMLInputElement).value) || 22)}
                class={`w-full px-4 py-2 bg-gray-700 border rounded-lg text-white focus:outline-none ${
                  validationErrors.i2cSCL || validationErrors.i2cPins ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
                }`}
              />
              {validationErrors.i2cSCL && (
                <p class="mt-1 text-sm text-red-400">{validationErrors.i2cSCL}</p>
              )}
            </div>
          </div>
          {validationErrors.i2cPins && (
            <p class="text-sm text-red-400">{validationErrors.i2cPins}</p>
          )}
          <p class="text-xs text-gray-500">
            Common: SDA=21, SCL=22. Valid GPIOs: 0,1,2,3,4,5,12-19,21-23,25-27,32-36,39
          </p>
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              I2C Frequency (Hz)
            </label>
            <select
              value={config.sensor.i2cFrequency}
              onChange={(e) => updateConfig(['sensor', 'i2cFrequency'], parseInt((e.target as HTMLSelectElement).value))}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500"
            >
              <option value="10000">10 kHz (Slow)</option>
              <option value="50000">50 kHz</option>
              <option value="100000">100 kHz (Standard)</option>
              <option value="400000">400 kHz (Fast)</option>
            </select>
            {validationErrors.i2cFrequency && (
              <p class="mt-1 text-sm text-red-400">{validationErrors.i2cFrequency}</p>
            )}
          </div>
        </div>
      </section>

      {/* Save Button */}
      <div class="flex justify-end">
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
