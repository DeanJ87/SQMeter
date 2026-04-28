#pragma once

#include <string>
#include <optional>
#include <cstdint>

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

    struct SensorConfig
    {
        uint32_t readIntervalMs;
        uint8_t i2cSDA;
        uint8_t i2cSCL;
        uint32_t i2cFrequency;
    };

    struct CloudDetectionConfig
    {
        float clearSkyThreshold;    // °C, corrected delta below which sky is clear (default: -13.0)
        float cloudyThreshold;      // °C, corrected delta above which sky is overcast (default: -3.0)
        float humidityCorrection;   // k1 factor for humidity correction (default: 0.75)
    };

    struct Config
    {
        WiFiConfig wifi;
        MQTTConfig mqtt;
        NTPConfig ntp;
        GPSConfig gps;
        SensorConfig sensor;
        CloudDetectionConfig cloudDetection;
        std::string deviceName;
        std::string timezone;
        TimeSource primaryTimeSource;   // Primary time source
        TimeSource secondaryTimeSource; // Fallback time source

        static constexpr const char *TAG = "Config";

        static std::optional<Config> load();
        bool save() const;
        static Config createDefault();

        std::string toJson() const;
        static std::optional<Config> fromJson(const std::string &json);
    };

} // namespace SQM
