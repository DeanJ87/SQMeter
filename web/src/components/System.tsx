import { FunctionalComponent } from 'preact';
import { getTimezoneFriendlyName } from '../utils/timezone';
import { useWebSocket } from '../hooks/useWebSocket';
import type { SystemStatus } from '../types';

const System: FunctionalComponent = () => {
  const { data: status, connected } = useWebSocket<SystemStatus>('/ws/status');

  if (!connected || !status) {
    return (
      <div class="flex items-center justify-center min-h-[60vh]">
        <div class="text-center">
          <div class="text-6xl mb-4">⏳</div>
          <h2 class="text-2xl font-bold text-white mb-2">Loading...</h2>
          <p class="text-gray-400">Connecting to device...</p>
        </div>
      </div>
    );
  }

  const handleRestart = async () => {
    if (!confirm('Are you sure you want to restart the device?')) return;

    try {
      await fetch('/api/restart', { method: 'POST' });
      alert('Device is restarting...');
    } catch (error) {
      alert('Failed to restart device');
    }
  };

  const formatUptime = (seconds: number): string => {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${days}d ${hours}h ${minutes}m ${secs}s`;
  };

  const formatBytes = (bytes: number): string => {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1048576) return `${(bytes / 1024).toFixed(2)} KB`;
    return `${(bytes / 1048576).toFixed(2)} MB`;
  };

  const getSensorStatusBadge = (status: number): { text: string; color: string } => {
    switch (status) {
      case 0: return { text: 'OK', color: 'bg-green-900 text-green-200' };
      case 1: return { text: 'Not Initialized', color: 'bg-yellow-900 text-yellow-200' };
      case 2: return { text: 'Error', color: 'bg-red-900 text-red-200' };
      case 3: return { text: 'Timeout', color: 'bg-orange-900 text-orange-200' };
      case 4: return { text: 'Invalid Data', color: 'bg-red-900 text-red-200' };
      default: return { text: 'Unknown', color: 'bg-gray-900 text-gray-200' };
    }
  };

  const heapUsedPercent = ((status.heapSize - status.freeHeap) / status.heapSize) * 100;

  return (
    <div class="max-w-4xl mx-auto space-y-6">
      <h1 class="text-3xl font-bold text-white mb-6">System Information</h1>

      {/* Firmware Info */}
      {status.firmware && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">Firmware</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <div class="text-sm text-gray-400 mb-1">Name</div>
              <div class="text-lg font-semibold text-white">
                {status.firmware.name}
              </div>
            </div>
            <div>
              <div class="text-sm text-gray-400 mb-1">Version</div>
              <div class="text-lg font-semibold text-white">
                v{status.firmware.version}
              </div>
            </div>
            <div class="md:col-span-2">
              <div class="text-sm text-gray-400 mb-1">Build Date</div>
              <div class="text-lg font-semibold text-white font-mono">
                {status.firmware.buildDate} {status.firmware.buildTime}
              </div>
            </div>
          </div>
        </section>
      )}

      {/* System Status */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Status</h2>
        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div>
            <div class="text-sm text-gray-400 mb-1">Uptime</div>
            <div class="text-lg font-semibold text-white">
              {formatUptime(status.uptime)}
            </div>
          </div>
          <div>
            <div class="text-sm text-gray-400 mb-1">
              Current Time
              {status.ntp && status.ntp.activeSource > 0 && (
                <span class={`ml-2 px-2 py-0.5 rounded text-xs font-semibold ${
                  status.ntp.activeSource === 1 ? 'bg-blue-900 text-blue-200' : 'bg-green-900 text-green-200'
                }`}>
                  {status.ntp.activeSource === 1 ? '📡 NTP' : '📍 GPS'}
                </span>
              )}
            </div>
            <div class="text-lg font-semibold text-white font-mono">
              {status.time.iso}
            </div>
            <div class="text-xs text-gray-500 mt-1">
              {getTimezoneFriendlyName(status.time.timezone)}
            </div>
          </div>
          <div>
            <div class="text-sm text-gray-400 mb-1">CPU Frequency</div>
            <div class="text-lg font-semibold text-white">
              {status.cpuFreqMHz} MHz
            </div>
          </div>
          <div>
            <div class="text-sm text-gray-400 mb-1">Memory (Heap)</div>
            <div class="text-lg font-semibold text-white">
              {formatBytes(status.freeHeap)} / {formatBytes(status.heapSize)}
            </div>
            <div class="w-full bg-gray-700 rounded-full h-2 mt-2">
              <div
                class="bg-blue-600 h-2 rounded-full transition-all"
                style={{ width: `${100 - heapUsedPercent}%` }}
              />
            </div>
          </div>
          <div>
            <div class="text-sm text-gray-400 mb-1">Flash (Program)</div>
            <div class="text-lg font-semibold text-white">
              {formatBytes(status.sketchSize)} / {formatBytes(status.flashSize)}
            </div>
            <div class="w-full bg-gray-700 rounded-full h-2 mt-2">
              <div
                class="bg-green-600 h-2 rounded-full transition-all"
                style={{ width: `${(status.sketchSize / status.flashSize) * 100}%` }}
              />
            </div>
            <div class="text-xs text-gray-500 mt-1">
              Running: {status.partitions?.runningSlot || 'Unknown'} • Free: {formatBytes(status.freeSketchSpace || 0)}
            </div>
          </div>
          <div>
            <div class="text-sm text-gray-400 mb-1">Filesystem (LittleFS)</div>
            <div class="text-lg font-semibold text-white">
              {formatBytes(status.fsUsed)} / {formatBytes(status.fsTotal)}
            </div>
            <div class="w-full bg-gray-700 rounded-full h-2 mt-2">
              <div
                class="bg-purple-600 h-2 rounded-full transition-all"
                style={{ width: `${(status.fsUsed / status.fsTotal) * 100}%` }}
              />
            </div>
            <div class="text-xs text-gray-500 mt-1">
              Partition Size: {formatBytes(status.partitions?.fsSize || status.fsTotal)}
            </div>
          </div>
        </div>
      </section>

      {/* Partition Details */}
      {status.partitions && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">Flash Partitions</h2>
          <div class="space-y-4">
            {/* OTA Partitions */}
            <div class="bg-gray-900 rounded-lg p-4">
              <h3 class="text-sm font-semibold text-gray-300 mb-3">OTA (Over-The-Air Update)</h3>
              <div class="grid grid-cols-2 gap-4">
                <div>
                  <div class="text-xs text-gray-500">Current Slot</div>
                  <div class="text-white font-mono text-sm">{status.partitions.runningSlot}</div>
                  <div class="text-xs text-gray-500 mt-1">{formatBytes(status.partitions.runningSize)}</div>
                </div>
                <div>
                  <div class="text-xs text-gray-500">Next Update Slot</div>
                  <div class="text-white font-mono text-sm">{status.partitions.nextSlot}</div>
                  <div class="text-xs text-gray-500 mt-1">{formatBytes(status.partitions.nextSize)}</div>
                </div>
              </div>
              <div class="text-xs text-gray-400 mt-2">
                ℹ️ Firmware alternates between app0 and app1 partitions for safe updates
              </div>
            </div>

            {/* NVS Partition */}
            {status.partitions.nvs && (
              <div class="bg-gray-900 rounded-lg p-4">
                <h3 class="text-sm font-semibold text-gray-300 mb-3">NVS (Non-Volatile Storage)</h3>
                <div class="grid grid-cols-3 gap-4">
                  <div>
                    <div class="text-xs text-gray-500">Used Entries</div>
                    <div class="text-white font-semibold">{status.partitions.nvs.usedEntries}</div>
                  </div>
                  <div>
                    <div class="text-xs text-gray-500">Free Entries</div>
                    <div class="text-white font-semibold">{status.partitions.nvs.freeEntries}</div>
                  </div>
                  <div>
                    <div class="text-xs text-gray-500">Namespaces</div>
                    <div class="text-white font-semibold">{status.partitions.nvs.namespaceCount}</div>
                  </div>
                </div>
                <div class="w-full bg-gray-700 rounded-full h-2 mt-3">
                  <div
                    class="bg-yellow-600 h-2 rounded-full transition-all"
                    style={{ width: `${(status.partitions.nvs.usedEntries / status.partitions.nvs.totalEntries) * 100}%` }}
                  />
                </div>
                <div class="text-xs text-gray-400 mt-2">
                  📝 Stores configuration, WiFi credentials, and settings
                </div>
              </div>
            )}
          </div>
        </section>
      )}

      {/* NTP Status */}
      {status.ntp && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">Network Time (NTP)</h2>
          {status.ntp.enabled ? (
            <div class="space-y-3">
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Sync Status</span>
                <span class={`px-3 py-1 rounded-full text-sm font-semibold ${
                  status.ntp.synced ? 'bg-green-900 text-green-200' : 
                  status.ntp.status === 1 ? 'bg-yellow-900 text-yellow-200' :
                  'bg-red-900 text-red-200'
                }`}>
                  {status.ntp.synced ? '✓ Synced' : 
                   status.ntp.status === 1 ? '⏳ Syncing...' :
                   status.ntp.status === 3 ? '✗ Failed' : '○ Not Synced'}
                </span>
              </div>
              {status.ntp.synced && (
                <>
                  <div class="flex justify-between items-center">
                    <span class="text-gray-400">Last Sync</span>
                    <span class="text-white font-medium">
                      {status.ntp.lastSync > 0 ? `${Math.floor((status.uptime * 1000 - status.ntp.lastSync) / 1000)}s ago` : 'Never'}
                    </span>
                  </div>
                  <div class="flex justify-between items-center">
                    <span class="text-gray-400">Next Sync</span>
                    <span class="text-white font-medium">
                      {status.ntp.nextSync > 0 ? `in ${Math.floor(status.ntp.nextSync / 1000)}s` : 'Calculating...'}
                    </span>
                  </div>
                  <div class="flex justify-between items-center">
                    <span class="text-gray-400">Clock Drift</span>
                    <span class="text-white font-medium">
                      {status.ntp.drift}s
                    </span>
                  </div>
                </>
              )}
              <div class="flex justify-between items-center">
                <span class="text-gray-400">NTP Server</span>
                <span class="text-white font-mono text-sm">{status.ntp.server}</span>
              </div>
            </div>
          ) : (
            <div class="text-gray-400">NTP is disabled. Enable in Settings to sync time automatically.</div>
          )}
        </section>
      )}

      {/* GPS Time Status */}
      {status.ntp && status.ntp.gpsEnabled && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">GPS Time</h2>
          <div class="space-y-3">
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Status</span>
              <span class={`px-3 py-1 rounded-full text-sm font-semibold ${
                status.ntp.gpsHasFix ? 'bg-green-900 text-green-200' : 'bg-yellow-900 text-yellow-200'
              }`}>
                {status.ntp.gpsHasFix ? '✓ Fix Acquired' : '⏳ Searching...'}
              </span>
            </div>
            
            {/* GPS UTC TIME - PROMINENT DISPLAY */}
            {status.ntp.gpsHasFix && status.ntp.gpsTimeUTC && (
              <div class="bg-gray-700/50 rounded-lg p-4 border border-gray-600">
                <div class="text-sm text-gray-400 mb-1">GPS Time (UTC)</div>
                <div class="text-2xl font-bold text-white font-mono">
                  {status.ntp.gpsTimeUTC}
                </div>
              </div>
            )}
            
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Satellites</span>
              <span class="text-white font-medium">
                {status.ntp.gpsSatellites || 0}
              </span>
            </div>
            
            {status.ntp.gpsHasFix && status.gpsData && (
              <>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Time Update Age</span>
                  <span class="text-white font-medium">
                    {(status.gpsData.age / 1000).toFixed(1)}s
                  </span>
                </div>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Location Accuracy (HDOP)</span>
                  <span class="text-white font-medium">
                    {status.gpsData.hdop.toFixed(1)}
                  </span>
                </div>
              </>
            )}
          </div>
        </section>
      )}

      {/* MQTT Status */}
      {status.mqtt && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">MQTT</h2>
          {status.mqtt.enabled ? (
            <div class="space-y-3">
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Connection Status</span>
                <span class={`px-3 py-1 rounded-full text-sm font-semibold ${
                  status.mqtt.connected ? 'bg-green-900 text-green-200' : 
                  status.mqtt.lastReconnectAttempt > 0 && (status.uptime * 1000 - status.mqtt.lastReconnectAttempt) < 5000 ? 'bg-yellow-900 text-yellow-200' :
                  'bg-red-900 text-red-200'
                }`}>
                  {status.mqtt.connected ? '✓ Connected' : 
                   status.mqtt.lastReconnectAttempt > 0 && (status.uptime * 1000 - status.mqtt.lastReconnectAttempt) < 5000 ? '🔄 Reconnecting...' :
                   '✗ Disconnected'}
                </span>
              </div>
              {!status.mqtt.connected && (
                <>
                  <div class="flex justify-between items-center">
                    <span class="text-gray-400">State Code</span>
                    <span class="text-white font-mono">
                      {status.mqtt.state === -4 ? '-4 (Timeout)' :
                       status.mqtt.state === -3 ? '-3 (Connection Lost)' :
                       status.mqtt.state === -2 ? '-2 (Connect Failed)' :
                       status.mqtt.state === -1 ? '-1 (Disconnected)' :
                       status.mqtt.state === 1 ? '1 (Bad Protocol)' :
                       status.mqtt.state === 2 ? '2 (Bad Client ID)' :
                       status.mqtt.state === 3 ? '3 (Unavailable)' :
                       status.mqtt.state === 4 ? '4 (Bad Credentials)' :
                       status.mqtt.state === 5 ? '5 (Unauthorized)' :
                       status.mqtt.state}
                    </span>
                  </div>
                  <div class="flex justify-between items-center">
                    <span class="text-gray-400">Last Reconnect Attempt</span>
                    <span class="text-white font-medium">
                      {status.mqtt.lastReconnectAttempt > 0 
                        ? `${Math.floor((status.uptime * 1000 - status.mqtt.lastReconnectAttempt) / 1000)}s ago` 
                        : 'Never'}
                    </span>
                  </div>
                </>
              )}
              {status.mqtt.connected && (
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Last Publish</span>
                  <span class="text-white font-medium">
                    {status.mqtt.lastPublish > 0 
                      ? `${Math.floor((status.uptime * 1000 - status.mqtt.lastPublish) / 1000)}s ago` 
                      : 'No data published yet'}
                  </span>
                </div>
              )}
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Broker</span>
                <span class="text-white font-mono text-sm">{status.mqtt.broker}:{status.mqtt.port}</span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Topic</span>
                <span class="text-white font-mono text-sm">{status.mqtt.topic}</span>
              </div>
            </div>
          ) : (
            <div class="text-gray-400">MQTT is disabled. Enable in Settings to publish sensor data.</div>
          )}
        </section>
      )}

      {/* Sensor Status */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Sensors</h2>
        <div class="space-y-3">
          <div class="flex justify-between items-center">
            <div>
              <span class="text-white font-medium">TSL2591 (Light Sensor)</span>
              <div class="text-xs text-gray-500">
                {status.sensors.tsl2591.initialized ? '✓ Initialized' : '✗ Not Initialized'} • Last update: {status.sensors.tsl2591.lastUpdate}ms
              </div>
            </div>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${getSensorStatusBadge(status.sensors.tsl2591.status).color}`}>
              {getSensorStatusBadge(status.sensors.tsl2591.status).text}
            </span>
          </div>
          <div class="flex justify-between items-center">
            <div>
              <span class="text-white font-medium">BME280 (Environment)</span>
              <div class="text-xs text-gray-500">
                {status.sensors.bme280.initialized ? '✓ Initialized' : '✗ Not Initialized'} • Last update: {status.sensors.bme280.lastUpdate}ms
              </div>
            </div>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${getSensorStatusBadge(status.sensors.bme280.status).color}`}>
              {getSensorStatusBadge(status.sensors.bme280.status).text}
            </span>
          </div>
          <div class="flex justify-between items-center">
            <div>
              <span class="text-white font-medium">MLX90614 (IR Temperature)</span>
              <div class="text-xs text-gray-500">
                {status.sensors.mlx90614.initialized ? '✓ Initialized' : '✗ Not Initialized'} • Last update: {status.sensors.mlx90614.lastUpdate}ms
              </div>
            </div>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${getSensorStatusBadge(status.sensors.mlx90614.status).color}`}>
              {getSensorStatusBadge(status.sensors.mlx90614.status).text}
            </span>
          </div>
          <div class="flex justify-between items-center">
            <div>
              <span class="text-white font-medium">GPS Module (Ublox Neo-6M)</span>
              <div class="text-xs text-gray-500">
                {status.sensors.gps.initialized ? '✓ Initialized' : '✗ Not Initialized'} • Last update: {status.sensors.gps.lastUpdate}ms
              </div>
            </div>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${getSensorStatusBadge(status.sensors.gps.status).color}`}>
              {getSensorStatusBadge(status.sensors.gps.status).text}
            </span>
          </div>
        </div>
      </section>

      {/* GPS Data */}
      {status.gpsData && status.sensors.gps.initialized && (
        <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <h2 class="text-xl font-semibold text-white mb-4">GPS Location</h2>
          <div class="space-y-3">
            <div class="flex justify-between items-center">
              <span class="text-gray-400">GPS Fix</span>
              <span class={`px-3 py-1 rounded-full text-sm font-semibold ${
                status.gpsData.hasFix ? 'bg-green-900 text-green-200' : 'bg-yellow-900 text-yellow-200'
              }`}>
                {status.gpsData.hasFix ? '✓ Lock Acquired' : '⏳ Searching...'}
              </span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Satellites</span>
              <span class="text-white font-medium">{status.gpsData.satellites}</span>
            </div>
            {status.gpsData.hasFix && (
              <>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Latitude</span>
                  <span class="text-white font-mono">{status.gpsData.latitude.toFixed(6)}°</span>
                </div>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Longitude</span>
                  <span class="text-white font-mono">{status.gpsData.longitude.toFixed(6)}°</span>
                </div>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Altitude</span>
                  <span class="text-white font-medium">{status.gpsData.altitude.toFixed(1)} m</span>
                </div>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">HDOP (Accuracy)</span>
                  <span class="text-white font-medium">{status.gpsData.hdop.toFixed(2)}</span>
                </div>
                <div class="flex justify-between items-center">
                  <span class="text-gray-400">Fix Age</span>
                  <span class="text-white font-medium">{(status.gpsData.age / 1000).toFixed(1)}s</span>
                </div>
              </>
            )}
          </div>
        </section>
      )}

      {/* WiFi Status */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">WiFi</h2>
        <div class="space-y-3">
          <div class="flex justify-between items-center">
            <span class="text-gray-400">Status</span>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${
              status.wifi.connected ? 'bg-green-900 text-green-200' : 'bg-red-900 text-red-200'
            }`}>
              {status.wifi.connected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
          {status.wifi.connected && (
            <>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">SSID</span>
                <span class="text-white font-medium">{status.wifi.ssid}</span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">IP Address</span>
                <span class="text-white font-mono">{status.wifi.ip}</span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Signal Strength</span>
                <span class="text-white font-medium">{status.wifi.rssi} dBm</span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">MAC Address</span>
                <span class="text-white font-mono text-sm">{status.wifi.mac}</span>
              </div>
            </>
          )}
        </div>
      </section>

      {/* System Actions */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Actions</h2>
        <div class="space-y-3">
          <button
            onClick={handleRestart}
            class="w-full px-6 py-3 bg-red-600 hover:bg-red-700 text-white font-semibold rounded-lg transition-colors"
          >
            Restart Device
          </button>
        </div>
      </section>
    </div>
  );
};

export default System;
