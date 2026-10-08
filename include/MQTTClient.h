#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/RG15Sensor.h"
#include <PubSubClient.h>
#include <functional>
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
        std::string availabilityTopic;
        std::string clientId;
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
        void publishSensorData(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps, const RG15Sensor &rg15);

        // Publish to <configured topic>/<subtopic>. Must be called from the
        // same task that runs handle() - PubSubClient isn't thread-safe.
        bool publishSubtopic(const std::string &subtopic, const std::string &payload, bool retained);

        // Update configuration
        void updateConfig(const MQTTConfig &newConfig);

        // Commands from the broker: `<topic>/<subtopic>` messages go to
        // `handler` (on the task that runs handle()). Subscribed on every
        // (re)connect.
        using CommandHandler = std::function<void(const std::string &payload)>;
        void onCommand(const std::string &subtopic, CommandHandler handler);
        // Bumped on every successful connect, so retained state can be
        // republished after the broker comes back.
        uint32_t connectionCount() const { return connections; }

        // The SafetyMonitor verdict, added to the readings payload as
        // "safe": 1/0 so it's logged alongside them.
        void setSafety(bool safe) { safeState = safe ? 1 : 0; }

    private:
        int8_t safeState = -1; // -1 until the first verdict
        std::string commandSubtopic;
        CommandHandler commandHandler;
        uint32_t connections = 0;
        static constexpr const char *TAG = "MQTT";
        static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

        MQTTConfig config;
        WiFiClient wifiClient;
        std::unique_ptr<PubSubClient> mqttClient;

        uint32_t lastReconnectAttempt;
        uint32_t lastPublish;

        void connect();
        void reconnect();
        void publishAvailability(bool online);
        std::string buildClientId() const;
        std::string getAvailabilityTopic() const;

        std::string createPayload(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps, const RG15Sensor &rg15);
    };

} // namespace SQM
