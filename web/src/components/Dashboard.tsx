import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import type { Config, SensorData } from '../types';

const Dashboard: FunctionalComponent = () => {
  const { data: sensors, connected, lastMessageAt } = useWebSocket<SensorData>('/ws/sensors');
  const [config, setConfig] = useState<Config | null>(null);

  useEffect(() => {
    fetch('/api/config')
      .then((response) => (response.ok ? response.json() : null))
      .then((data) => setConfig(data))
      .catch(() => setConfig(null));
  }, []);

  const getBortleColor = (bortle: number): string => {
    if (bortle <= 2) return 'text-green-400';
    if (bortle <= 4) return 'text-blue-400';
    if (bortle <= 6) return 'text-yellow-400';
    if (bortle <= 8) return 'text-orange-400';
    return 'text-red-400';
  };

  const formatNumber = (value: number | undefined, digits: number) =>
    typeof value === 'number' && Number.isFinite(value) ? value.toFixed(digits) : '--';
  const formatAgeMs = (value: number | null | undefined) => {
    if (typeof value !== 'number' || !Number.isFinite(value)) return '--';
    if (value < 1000) return `${value} ms`;
    if (value < 60000) return `${(value / 1000).toFixed(1)} s`;
    return `${Math.floor(value / 60000)}m ${Math.floor((value % 60000) / 1000)}s`;
  };

  const rainUnits = config?.rain?.units === 'imperial'
    ? { depth: 'in', intensity: 'in/hr' }
    : { depth: 'mm', intensity: 'mm/hr' };
  const rain = sensors?.rainSensor;

  const rainStatus = !rain?.enabled
    ? { text: 'Disabled', color: 'bg-gray-900 text-gray-300' }
    : rain.stale
      ? { text: 'Stale', color: 'bg-yellow-900 text-yellow-200' }
      : rain.online
        ? { text: 'Online', color: 'bg-green-900 text-green-200' }
        : { text: 'Offline', color: 'bg-red-900 text-red-200' };

  const payloadTimestamp = sensors?.dataTimestamp;
  const dataTimestamp = payloadTimestamp && payloadTimestamp > 1000000000000
    ? payloadTimestamp
    : lastMessageAt;
  const dataAgeSeconds = dataTimestamp ? Math.max(0, Math.floor((Date.now() - dataTimestamp) / 1000)) : null;
  const isStale = dataAgeSeconds !== null && config?.sensor?.readIntervalMs
    ? dataAgeSeconds * 1000 > config.sensor.readIntervalMs * 2
    : false;

  if (!connected) {
    return (
      <div class="flex items-center justify-center min-h-[60vh]">
        <div class="text-center">
          <div class="text-6xl mb-4">🔌</div>
          <h2 class="text-2xl font-bold text-white mb-2">Connecting...</h2>
          <p class="text-gray-400">Establishing WebSocket connection</p>
        </div>
      </div>
    );
  }

  if (!sensors) {
    return (
      <div class="flex items-center justify-center min-h-[60vh]">
        <div class="text-center">
          <div class="text-6xl mb-4">⏳</div>
          <h2 class="text-2xl font-bold text-white mb-2">Loading...</h2>
          <p class="text-gray-400">Waiting for sensor data</p>
        </div>
      </div>
    );
  }

  return (
    <div class="space-y-6">
      {/* Connection Status */}
      <div class="bg-gray-800 rounded-lg p-4 border border-gray-700">
        <div class="flex flex-wrap items-center gap-3">
          <div class={`w-3 h-3 rounded-full ${connected && !isStale ? 'bg-green-500' : 'bg-red-500'} animate-pulse`} />
          <span class="text-sm text-gray-400">
            {connected ? 'Connected' : 'Disconnected'}
          </span>
          {dataAgeSeconds !== null && (
            <span class={`text-sm ${isStale ? 'text-yellow-300' : 'text-gray-400'}`}>
              Data age: {dataAgeSeconds}s
            </span>
          )}
        </div>
      </div>

      {/* Sky Quality - Primary Display */}
      {sensors.skyQuality ? (
        <div class="bg-gradient-to-br from-indigo-900 to-purple-900 rounded-xl p-8 shadow-2xl border border-indigo-700">
        <div class="text-center">
          <div class="text-6xl mb-4">🌌</div>
          <h2 class="text-3xl font-bold text-white mb-2">Sky Quality</h2>
          <div class="mt-6 space-y-4">
            <div>
              <div class="text-6xl font-bold text-white mb-2">
                {formatNumber(sensors.skyQuality.sqm, 2)}
              </div>
              <div class="text-xl text-gray-300">mag/arcsec²</div>
            </div>
            <div class={`text-2xl font-semibold ${getBortleColor(sensors.skyQuality.bortle)}`}>
              {sensors.skyQuality.description}
            </div>
            <div class="grid grid-cols-2 gap-4 mt-6 max-w-md mx-auto">
              <div class="bg-black bg-opacity-30 rounded-lg p-4">
                <div class="text-sm text-gray-400">NELM</div>
                <div class="text-2xl font-bold text-white">
                  {formatNumber(sensors.skyQuality.nelm, 1)}
                </div>
              </div>
              <div class="bg-black bg-opacity-30 rounded-lg p-4">
                <div class="text-sm text-gray-400">Bortle</div>
                <div class="text-2xl font-bold text-white">
                  {formatNumber(sensors.skyQuality.bortle, 1)}
                </div>
              </div>
            </div>
          </div>
        </div>
        </div>
      ) : (
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700 text-gray-300">
          Sky quality data unavailable
        </div>
      )}

      {/* Cloud Conditions */}
      {sensors.cloudConditions && (
        <div class="bg-gradient-to-br from-blue-900 to-indigo-900 rounded-xl p-8 shadow-2xl border border-blue-700">
          <div class="text-center">
            <div class="text-6xl mb-4">
              {sensors.cloudConditions.condition === 1 ? '☀️' : 
               sensors.cloudConditions.condition === 2 ? '⛅' : 
               sensors.cloudConditions.condition === 3 ? '☁️' : '❓'}
            </div>
            <h2 class="text-3xl font-bold text-white mb-2">Cloud Conditions</h2>
            <div class="text-2xl font-semibold text-blue-200 mb-6">
              {sensors.cloudConditions.description}
            </div>
            <div class="grid grid-cols-3 gap-4 max-w-2xl mx-auto">
              <div class="bg-black bg-opacity-30 rounded-lg p-4">
                <div class="text-sm text-gray-400">Cloud Cover</div>
                <div class="text-2xl font-bold text-white">
                  {sensors.cloudConditions.cloudCoverPercent.toFixed(0)}%
                </div>
              </div>
              <div class="bg-black bg-opacity-30 rounded-lg p-4">
                <div class="text-sm text-gray-400">Temp Delta</div>
                <div class="text-2xl font-bold text-white">
                  {sensors.cloudConditions.temperatureDelta.toFixed(1)}°C
                </div>
              </div>
              <div class="bg-black bg-opacity-30 rounded-lg p-4">
                <div class="text-sm text-gray-400">Corrected Δ</div>
                <div class="text-2xl font-bold text-white">
                  {sensors.cloudConditions.correctedDelta.toFixed(1)}°C
                </div>
                <div class="text-xs text-gray-400 mt-1">RH: {sensors.cloudConditions.humidityUsed.toFixed(0)}%</div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* GPS Location */}
      {sensors.gps && sensors.gps.hasFix && (
        <div class="bg-gradient-to-br from-green-900 to-emerald-900 rounded-xl p-8 shadow-2xl border border-green-700">
          <div class="text-center">
            <div class="text-6xl mb-4">📍</div>
            <h2 class="text-3xl font-bold text-white mb-2">GPS Location</h2>
            <div class="text-xl font-semibold text-green-200 mb-6">
              <span>
                {Math.abs(sensors.gps.latitude).toFixed(6)}° {sensors.gps.latitude >= 0 ? 'N' : 'S'}, {Math.abs(sensors.gps.longitude).toFixed(6)}° {sensors.gps.longitude >= 0 ? 'E' : 'W'}
              </span>
            </div>
            
            {
              <div class="grid grid-cols-2 md:grid-cols-4 gap-4 max-w-3xl mx-auto">
                <div class="bg-black bg-opacity-30 rounded-lg p-4">
                  <div class="text-sm text-gray-400">Satellites</div>
                  <div class="text-2xl font-bold text-white">
                    {sensors.gps.satellites}
                  </div>
                </div>
                <div class="bg-black bg-opacity-30 rounded-lg p-4">
                  <div class="text-sm text-gray-400">Altitude</div>
                  <div class="text-2xl font-bold text-white">
                    {sensors.gps.altitude.toFixed(0)} m
                  </div>
                </div>
                <div class="bg-black bg-opacity-30 rounded-lg p-4">
                  <div class="text-sm text-gray-400">HDOP</div>
                  <div class="text-2xl font-bold text-white">
                    {sensors.gps.hdop.toFixed(1)}
                  </div>
                </div>
                <div class="bg-black bg-opacity-30 rounded-lg p-4">
                  <div class="text-sm text-gray-400">Fix Age</div>
                  <div class="text-2xl font-bold text-white">
                    {(sensors.gps.age / 1000).toFixed(1)}s
                  </div>
                </div>
              </div>
            }
          </div>
        </div>
      )}

      {/* Rain Sensor */}
      {rain && (
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <div class="flex flex-wrap items-start justify-between gap-3 mb-4">
            <div>
              <h3 class="text-lg font-semibold text-white flex items-center">
                <span class="mr-2">🌧️</span>
                Rain Sensor
              </h3>
              <p class="text-xs text-gray-400 mt-1">Hydreon RG-15 live rainfall reading</p>
            </div>
            <span class={`px-3 py-1 rounded-full text-sm font-semibold ${rainStatus.color}`}>
              {rainStatus.text}
            </span>
          </div>

          <div class="grid grid-cols-2 md:grid-cols-4 gap-3 mb-5">
            <div class="bg-gray-900 rounded p-3">
              <div class="text-xs text-gray-500">Rain intensity</div>
              <div class="text-xl font-bold text-white">
                {rain.enabled ? formatNumber(rain.rain_intensity ?? rain.rInt, 1) : '--'} {rainUnits.intensity}
              </div>
            </div>
            <div class="bg-gray-900 rounded p-3">
              <div class="text-xs text-gray-500">Since last read</div>
              <div class="text-lg font-semibold text-white">{rain.enabled ? formatNumber(rain.accumulation_since_last_read ?? rain.acc, 2) : '--'} {rainUnits.depth}</div>
            </div>
            <div class="bg-gray-900 rounded p-3">
              <div class="text-xs text-gray-500">Event total</div>
              <div class="text-lg font-semibold text-white">{rain.enabled ? formatNumber(rain.event_accumulation ?? rain.eventAcc, 2) : '--'} {rainUnits.depth}</div>
            </div>
            <div class="bg-gray-900 rounded p-3">
              <div class="text-xs text-gray-500">Lifetime total</div>
              <div class="text-lg font-semibold text-white">{rain.enabled ? formatNumber(rain.total_accumulation ?? rain.totalAcc, 2) : '--'} {rainUnits.depth}</div>
            </div>
          </div>

          <div class="grid grid-cols-1 md:grid-cols-3 gap-3 text-sm text-gray-400">
            <div>Last read: <span class="text-white">{formatAgeMs(rain.uart?.last_successful_read_age_ms)}</span></div>
            <div>Mode: <span class="text-white capitalize">{rain.uart?.mode ?? '--'}</span></div>
            <div>Debug: <span class="text-white">System page</span></div>
          </div>

          <div class="mt-3">
            {(rain.lensBad || rain.emSat) && (
              <div class="pt-2 border-t border-gray-700 space-y-1">
                {rain.lensBad && (
                  <span class="block text-xs text-yellow-400">⚠ LensBad — lens may be dirty or obstructed</span>
                )}
                {rain.emSat && (
                  <span class="block text-xs text-yellow-400">⚠ EmSat — emitter saturation detected</span>
                )}
              </div>
            )}
          </div>
        </div>
      )}

      {/* Sensor Readings Grid */}
      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Light Sensor */}
        {sensors.lightSensor && (
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <div class="flex items-center justify-between mb-4">
            <h3 class="text-lg font-semibold text-white flex items-center">
              <span class="mr-2">💡</span>
              Light Sensor
            </h3>
          </div>
          <div class="space-y-3">
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Illuminance</span>
              <span class="text-xl font-bold text-white">
                {sensors.lightSensor.status === 0 ? formatNumber(sensors.lightSensor.lux, 6) : '--'} lux
              </span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Visible</span>
              <span class="text-white">{sensors.lightSensor.status === 0 ? sensors.lightSensor.visible : '--'}</span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Infrared</span>
              <span class="text-white">{sensors.lightSensor.status === 0 ? sensors.lightSensor.infrared : '--'}</span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-gray-400">Full Spectrum</span>
              <span class="text-white">{sensors.lightSensor.status === 0 ? sensors.lightSensor.full : '--'}</span>
            </div>
          </div>
        </div>
        )}

        {/* Sky Temperature Sensor */}
        {sensors.irTemperature && (
          <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
            <h3 class="text-lg font-semibold text-white flex items-center mb-4">
              <span class="mr-2">🌡️</span>
              IR Temperature
            </h3>
            <div class="space-y-3">
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Sky Temperature</span>
                <span class="text-xl font-bold text-white">
                  {sensors.irTemperature.status === 0 ? sensors.irTemperature.objectTemp?.toFixed(1) : '--'}°C
                </span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Ambient Temperature</span>
                <span class="text-white">
                  {sensors.irTemperature.status === 0 ? sensors.irTemperature.ambientTemp?.toFixed(1) : '--'}°C
                </span>
              </div>
            </div>
          </div>
        )}

        {/* Environmental Sensor - Only show if environment sensor is connected */}
        {sensors.environment && sensors.environment.status === 0 && (
          <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
            <h3 class="text-lg font-semibold text-white flex items-center mb-4">
              <span class="mr-2">🌤️</span>
              Environment
            </h3>
            <div class="space-y-3">
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Temperature</span>
                <span class="text-xl font-bold text-white">
                  {sensors.environment.temperature?.toFixed(1) ?? '--'}°C
                </span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Humidity</span>
                <span class="text-white">
                  {sensors.environment.humidity?.toFixed(1) ?? '--'}%
                </span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Pressure</span>
                <span class="text-white">
                  {sensors.environment.pressure?.toFixed(1) ?? '--'} hPa
                </span>
              </div>
              <div class="flex justify-between items-center">
                <span class="text-gray-400">Dew Point</span>
                <span class="text-white">
                  {sensors.environment.dewpoint?.toFixed(1) ?? '--'}°C
                </span>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default Dashboard;
