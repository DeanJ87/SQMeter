#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <memory>
#include <string>

namespace SQM
{

    struct MQTTStatus
    {
        bool enabled;
        bool connected;
        int state; // PubSubClient state code
        uint32_t lastPublishMs;
        uint32_t lastReconnectAttemptMs;
        std::string broker;
        int port;
        std::string topic;
    };

    class MQTTClient
    {
    public:
        explicit MQTTClient(const MQTTConfig &config);
        ~MQTTClient() = default;

        // Delete copy operations
        MQTTClient(const MQTTClient &) = delete;
        MQTTClient &operator=(const MQTTClient &) = delete;

        void begin();
        void handle();

        bool isConnected() const;
        bool isEnabled() const { return config.enabled; }

        // Get current status
        MQTTStatus getStatus() const;

        // Publish sensor data
        void publishSensorData(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps);

        // Update configuration
        void updateConfig(const MQTTConfig &newConfig);

    private:
        static constexpr const char *TAG = "MQTT";
        static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

        MQTTConfig config;
        WiFiClient wifiClient;
        std::unique_ptr<PubSubClient> mqttClient;

        uint32_t lastReconnectAttempt;
        uint32_t lastPublish;

        void connect();
        void reconnect();

        std::string createPayload(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps);
    };

} // namespace SQM
