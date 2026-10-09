#pragma once

#include "Config.h"
#include <PubSubClient.h>
#include <functional>
#include "DualStackClient.h"
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

        // Publish to <base topic>/<subtopic>, or to an absolute topic (Home
        // Assistant discovery). Must be called from the task that runs
        // handle() - PubSubClient isn't thread-safe. The readings themselves
        // are built by WebServer (lib/Readings).
        bool publishSubtopic(const std::string &subtopic, const std::string &payload, bool retained);
        bool publishTopic(const std::string &topic, const std::string &payload, bool retained);

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

    private:
        std::string commandSubtopic;
        CommandHandler commandHandler;
        uint32_t connections = 0;
        static constexpr const char *TAG = "MQTT";
        static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

        MQTTConfig config;
        DualStackClient wifiClient; // IPv4, or IPv6 literals and IPv6-only names (spec 015)
        // The broker as PubSubClient needs it: brackets off, a "[v6]:port" port applied.
        std::string brokerHost;
        uint16_t brokerPort = 0;
        void resolveBroker();
        std::unique_ptr<PubSubClient> mqttClient;

        uint32_t lastReconnectAttempt;
        uint32_t lastPublish;

        void connect();
        void reconnect();
        void publishAvailability(bool online);
        std::string buildClientId() const;
        std::string getAvailabilityTopic() const;
    };

} // namespace SQM
