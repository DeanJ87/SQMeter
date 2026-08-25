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
        std::string topic;
        uint32_t publishIntervalMs;
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
        std::string deviceName;
        std::string timezone;
        TimeSource primaryTimeSource;   // Primary time source
        TimeSource secondaryTimeSource; // Fallback time source

        static constexpr const char *TAG = "Config";
        static constexpr size_t MAX_PERSISTED_JSON_BYTES = 5100;

        static std::optional<Config> load();
        bool save() const;
        static Config createDefault();

        std::string toJson(bool redactSecrets = false) const;
        bool validate(std::string *error = nullptr) const;
        static std::optional<Config> fromJson(const std::string &json, const Config *baseConfig = nullptr);
    };

} // namespace SQM
