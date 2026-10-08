#include "MQTTClient.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

namespace SQM
{

    MQTTClient::MQTTClient(const MQTTConfig &config)
        : config(config),
          mqttClient(std::make_unique<PubSubClient>(wifiClient)),
          lastReconnectAttempt(0),
          lastPublish(0)
    {
    }

    void MQTTClient::begin()
    {
        if (!config.enabled)
        {
            Logger::info(TAG, "MQTT disabled");
            return;
        }

        Logger::info(TAG, "Initializing MQTT client");
        mqttClient->setServer(config.broker.c_str(), config.port);
        mqttClient->setBufferSize(3072); // Fits cloud and RG-15 diagnostics payloads
        mqttClient->setKeepAlive(60);
        mqttClient->setSocketTimeout(10);

        connect();
    }

    void MQTTClient::onCommand(const std::string &subtopic, CommandHandler handler)
    {
        commandSubtopic = subtopic;
        commandHandler = std::move(handler);
        mqttClient->setCallback(
            [this](char *topic, uint8_t *payload, unsigned int length)
            {
                if (commandHandler && config.topic + "/" + commandSubtopic == topic)
                    commandHandler(std::string(reinterpret_cast<const char *>(payload), length));
            });
        if (config.enabled && mqttClient->connected())
        {
            const std::string topic = config.topic + "/" + commandSubtopic;
            mqttClient->subscribe(topic.c_str(), 1);
        }
    }

    void MQTTClient::handle()
    {
        if (!config.enabled)
            return;

        if (!mqttClient->connected())
        {
            reconnect();
        }
        else
        {
            mqttClient->loop();
        }
    }

    bool MQTTClient::isConnected() const
    {
        return config.enabled && mqttClient->connected();
    }

    MQTTStatus MQTTClient::getStatus() const
    {
        MQTTStatus status;
        status.enabled = config.enabled;
        status.connected = mqttClient->connected();
        status.state = mqttClient->state();
        status.lastPublishMs = lastPublish;
        status.lastReconnectAttemptMs = lastReconnectAttempt;
        status.broker = config.broker;
        status.port = config.port;
        status.topic = config.topic;
        status.availabilityTopic = getAvailabilityTopic();
        status.clientId = buildClientId();
        return status;
    }

    void MQTTClient::updateConfig(const MQTTConfig &newConfig)
    {
        config = newConfig;

        if (config.enabled)
        {
            mqttClient->setServer(config.broker.c_str(), config.port);
            if (mqttClient->connected())
            {
                publishAvailability(false);
                mqttClient->disconnect();
            }
            connect();
        }
        else
        {
            if (mqttClient->connected())
            {
                publishAvailability(false);
            }
            mqttClient->disconnect();
        }
    }

    void MQTTClient::connect()
    {
        if (!config.enabled)
            return;

        Logger::info(TAG, "Connecting to MQTT broker: %s:%d", config.broker.c_str(), config.port);

        const std::string clientId = buildClientId();
        const std::string availabilityTopic = getAvailabilityTopic();

        bool connected = false;
        if (!config.username.empty())
        {
            connected = mqttClient->connect(
                clientId.c_str(), config.username.c_str(), config.password.c_str(), availabilityTopic.c_str(), 1, true, "offline");
        }
        else
        {
            connected = mqttClient->connect(clientId.c_str(), availabilityTopic.c_str(), 1, true, "offline");
        }

        if (connected)
        {
            Logger::info(TAG, "Connected to MQTT broker as %s", clientId.c_str());
            publishAvailability(true);
            if (commandHandler)
            {
                const std::string topic = config.topic + "/" + commandSubtopic;
                mqttClient->subscribe(topic.c_str(), 1);
            }
            ++connections;
        }
        else
        {
            Logger::error(TAG, "Failed to connect to MQTT broker, state: %d", mqttClient->state());
        }
    }

    void MQTTClient::reconnect()
    {
        const uint32_t now = millis();

        if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS)
        {
            return;
        }

        lastReconnectAttempt = now;
        Logger::info(TAG, "Attempting MQTT reconnection...");
        connect();
    }

    void MQTTClient::publishAvailability(bool online)
    {
        if (!config.enabled || !mqttClient->connected())
        {
            return;
        }

        const std::string availabilityTopic = getAvailabilityTopic();
        const char *payload = online ? "online" : "offline";
        if (!mqttClient->publish(availabilityTopic.c_str(), payload, true))
        {
            Logger::warn(TAG, "Failed to publish MQTT availability: %s", payload);
        }
    }

    std::string MQTTClient::buildClientId() const
    {
        const uint32_t macSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
        char id[24];
        snprintf(id, sizeof(id), "SQMeter-%06X", macSuffix);
        return std::string(id);
    }

    bool MQTTClient::publishSubtopic(const std::string &subtopic, const std::string &payload, bool retained)
    {
        if (!config.enabled || !mqttClient || !mqttClient->connected())
            return false;
        return publishTopic(config.topic + "/" + subtopic, payload, retained);
    }

    bool MQTTClient::publishTopic(const std::string &topic, const std::string &payload, bool retained)
    {
        if (!config.enabled || !mqttClient || !mqttClient->connected())
            return false;
        const bool ok = mqttClient->publish(topic.c_str(), reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), retained);
        if (ok)
            lastPublish = millis();
        return ok;
    }

    std::string MQTTClient::getAvailabilityTopic() const
    {
        return config.topic + "/availability";
    }

} // namespace SQM
