#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <cstddef>

namespace SQM
{

    enum class TimeSource
    {
        NTP = 0,
        GPS = 1
    };

    struct WiFiConfig
    {
        std::string ssid;
        std::string password;
        std::string hostname;
        bool mdns = true; // advertise <hostname>.local and the HTTP service
        bool autoReconnect;
        uint32_t reconnectDelayMs;
        uint32_t maxReconnectDelayMs;
    };

    struct MQTTConfig
    {
        bool enabled;
        std::string broker;
        uint16_t port;
        std::string username;
        std::string password;
        std::string topic; // base topic: <topic>/state, /safe, /alerts, ...
        uint32_t publishIntervalMs;
        // What <topic>/state (and the other topics) carry; see
        // specs/013-data-interfaces/contracts/mqtt-topics.md.
        struct Publish
        {
            bool sky = true;         // light + sky quality
            bool environment = true;
            bool clouds = true;      // IR temperatures + cloud cover
            bool gps = true;
            bool rain = true;
            bool wind = true;
            bool safety = true;      // <topic>/safe and <topic>/safety
            bool diagnostics = false; // <topic>/diagnostics
        } publish;
        bool homeAssistant = false; // MQTT discovery
        std::string discoveryPrefix = "homeassistant";
    };

    struct OTAConfig
    {
        bool enabled;
        std::string password;
    };

    struct AuthConfig
    {
        bool enabled;
        std::string username;
        std::string password;
    };

    struct NTPConfig
    {
        bool enabled;
        std::string server1;       // Primary NTP server (e.g., "pool.ntp.org")
        std::string server2;       // Secondary NTP server (optional fallback)
        std::string timezone;      // POSIX timezone string (e.g., "PST8PDT,M3.2.0,M11.1.0")
        int32_t gmtOffsetSec;      // GMT offset in seconds (e.g., -28800 for PST)
        int32_t daylightOffsetSec; // Daylight saving offset in seconds (e.g., 3600)
        uint32_t syncIntervalMs;   // How often to sync with NTP (default: 1 hour)
    };

    struct GPSConfig
    {
        bool enabled;
        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;
    };

    struct RainConfig
    {
        bool enabled;
        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;
        bool debugUart;
        std::string mode;       // retained for compatibility; RG-15 uses polling only
        std::string resolution; // "high", "low", or "switch"
        std::string units;      // "metric", "imperial", or "switch"
        uint32_t pollIntervalMs;
        uint32_t rainClearDelayMs;
        bool dailyResetEnabled;
        uint8_t dailyResetHour;
        uint8_t dailyResetMinute;
    };

    struct SensorConfig
    {
        uint32_t readIntervalMs;
        uint8_t i2cSDA;
        uint8_t i2cSCL;
        uint32_t i2cFrequency;
    };

    struct SkyAveragingConfig
    {
        uint16_t windowSeconds;
    };

    struct SkyCalibrationConfig
    {
        bool enabled;
        float sqmOffset;
        float darkVisibleOffset;
        float darkFullOffset;
        float darkIrOffset;
        uint32_t darkSampleCount;
        int64_t darkCalibratedAt;
    };

    struct CloudDetectionConfig
    {
        float clearSkyThreshold;    // °C, corrected delta below which sky is clear (default: -13.0)
        float cloudyThreshold;      // °C, corrected delta above which sky is overcast (default: -3.0)
        float humidityCorrection;   // k1 factor for humidity correction (default: 0.75)
    };

    // Thresholds for the native ASCOM Alpaca SafetyMonitor's "is it safe"
    // evaluation, ported from the SQMeter-ASCOM-Alpaca Go bridge's config.
    // Each *_enabled flag matches that bridge's "if configured" semantics -
    // disabled thresholds never contribute to an unsafe verdict.
    struct AlpacaConfig
    {
        bool enabled;                  // master switch for the Alpaca HTTP+UDP endpoints
        bool manualOverrideUnsafe;      // force SafetyMonitor.IsSafe = false regardless of readings
        uint32_t staleAfterSeconds;    // sensor data older than this counts as unsafe

        bool cloudCoverEnabled;
        float cloudCoverUnsafePercent;

        bool sqmMinEnabled;
        float sqmMinSafe;

        bool humidityMaxEnabled;
        float humidityMaxSafe;

        bool dewpointMarginEnabled;
        float dewpointMarginMinC;

        // Rain (RG-15). Rain is checked even when the other sensors are
        // stale - nothing should ever mask "it's raining".
        bool rainUnsafeEnabled;        // raining (incl. the rainClearDelayMs hold-off) => unsafe
        bool rainSensorRequired;       // rain sensor enabled but offline/stale/lens fault => unsafe

        // Wind (anemometer). Like rain, checked regardless of the other
        // sensors' freshness.
        bool windSpeedUnsafeEnabled;
        float windSpeedUnsafeMs;       // 2-minute mean wind speed
        bool windGustUnsafeEnabled;
        float windGustUnsafeMs;        // 10-minute peak gust

        // Conditions must stay continuously safe this long before IsSafe
        // flips back to true (0 = report safe immediately).
        uint32_t safeDelaySeconds;
    };

    // Push notifications for safety/rain/sensor events. Persisted under its
    // own NVS key (see Config::save) - ESP-IDF caps NVS strings at 4000
    // bytes, and the main config JSON is already close to that.
    struct AlertsConfig
    {
        bool enabled; // master switch

        // Per event: how loudly to alert (0 off, 1 quiet, 2 normal, 3 urgent,
        // 4 wake me - also rings paired phones) and an optional Pushover sound.
        struct EventSetting
        {
            uint8_t level;
            std::string sound;
            // Custom text with {variables}; empty uses the built-in wording.
            std::string title;
            std::string message;
        };
        static constexpr size_t MAX_TEMPLATE_TITLE = 80;
        static constexpr size_t MAX_TEMPLATE_MESSAGE = 240;
        EventSetting unsafe;
        EventSetting safe;
        EventSetting rainStarted;
        EventSetting rainStopped;
        EventSetting sensorFault;      // includes the RG-15 lens fault
        EventSetting sensorRecovered;
        EventSetting dewRisk;
        EventSetting clearSky;
        EventSetting cloudedOver;

        float dewRiskMarginC;          // temperature within this of the dew point
        float clearSkyCloudPercent;    // clear below this
        float cloudedOverCloudPercent; // clouded over above this
        bool skyNightOnly;          // sky alerts only while the sun is below nightSunAltitudeDeg
        bool safetyNightOnly;       // safe/unsafe alerts only then too
        bool armWithAlpaca;         // switch alerts on/off as N.I.N.A. connects/disconnects
        float nightSunAltitudeDeg;  // -0.833 sunset, -12 nautical, -18 astronomical
        uint32_t cooldownSeconds;   // min time between notifications of the same kind

        // Channels
        bool pushoverEnabled;
        std::string pushoverUserKey;
        std::string pushoverAppToken;
        std::string pushoverSound;  // default Pushover sound for events without their own

        bool ntfyEnabled;
        std::string ntfyServer;     // e.g. https://ntfy.sh
        std::string ntfyTopic;
        std::string ntfyToken;      // optional access token

        bool webhookEnabled;
        std::string webhookUrl;
        std::string webhookAuthHeader; // optional Authorization header value
        bool webhookInsecureTls;       // skip certificate checks (self-signed LAN servers only)

        bool mqttEnabled;           // publish to <mqtt topic>/alerts and retained <mqtt topic>/safety
    };

    // Cup anemometer + optional wind vane (see docs/hardware/wind.md).
    struct WindConfig
    {
        bool enabled;
        uint8_t speedPin;          // reed-switch anemometer, internal pull-up
        bool directionEnabled;
        uint8_t directionPin;      // resistor-ladder vane, must be an ADC1 pin (32-39)
        float kmhPerHz;            // 2.4 Misol/Argent/SparkFun, 3.621 Davis 6410
        float directionOffsetDeg;  // added to the vane reading to correct mounting
        float vanePullupOhms;      // vane divider pull-up to 3.3 V
    };

    // Observing site, for working out when it's dark. A GPS fix takes
    // precedence; this is the fallback.
    struct LocationConfig
    {
        bool set;
        double latitude;
        double longitude;
        bool showSunMoon; // Sun & Moon card on the dashboard
    };

    // Only used by the esp32dev-ble firmware build; ignored elsewhere.
    struct BleConfig
    {
        bool enabled; // advertise the SQMeter GATT service (takes effect after a restart)

        // Phone alarm service (pairing required). Without a passkey the
        // alarm/ack/heartbeat characteristics aren't offered at all. Events
        // set to "wake me" in the alert settings ring paired phones.
        std::string passkey; // 6 digits, entered on the phone when pairing
    };

    struct Config
    {
        WiFiConfig wifi;
        MQTTConfig mqtt;
        OTAConfig ota;
        AuthConfig auth;
        NTPConfig ntp;
        GPSConfig gps;
        RainConfig rain;
        SensorConfig sensor;
        SkyAveragingConfig skyAveraging;
        SkyCalibrationConfig skyCalibration;
        CloudDetectionConfig cloudDetection;
        AlpacaConfig alpaca;
        AlertsConfig alerts;
        BleConfig ble;
        WindConfig wind;
        LocationConfig location;
        std::string deviceName;
        std::string timezone;
        TimeSource primaryTimeSource;   // Primary time source
        TimeSource secondaryTimeSource; // Fallback time source

        static constexpr const char *TAG = "Config";
        static constexpr size_t MAX_PERSISTED_JSON_BYTES = 5100;

        // Loads the stored config into `out`. False (with `out` unspecified)
        // if nothing valid is stored.
        static bool load(Config &out);
        bool save() const;
        static Config createDefault();

        std::string toJson(bool redactSecrets = false, bool includeAlerts = true) const;
        std::string alertsToJson(bool redactSecrets = false) const;
        bool validate(std::string *error = nullptr) const;
        static std::optional<Config> fromJson(const std::string &json, const Config *baseConfig = nullptr);
        // Applies JSON on top of `cfg` in place and validates. On failure
        // `cfg` may be partially updated.
        static bool applyJson(const std::string &json, Config &cfg, bool preserveSecretPlaceholders);
    };

} // namespace SQM
