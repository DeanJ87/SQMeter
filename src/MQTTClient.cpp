#include "MQTTClient.h"
#include "Logger.h"
#include "calculations/SkyQuality.h"
#include "calculations/CloudDetection.h"
#include <ArduinoJson.h>

namespace SQM
{

    MQTTClient::MQTTClient(const MQTTConfig &config)
        : config(config), mqttClient(std::make_unique<PubSubClient>(wifiClient)), lastReconnectAttempt(0), lastPublish(0)
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
        mqttClient->setBufferSize(1024); // Increase buffer size for cloud data (default is 256)
        mqttClient->setKeepAlive(60);
        mqttClient->setSocketTimeout(10);

        connect();
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
        return status;
    }

    void MQTTClient::publishSensorData(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps)
    {
        if (!config.enabled || !mqttClient->connected())
        {
            return;
        }

        const uint32_t now = millis();
        if (now - lastPublish < config.publishIntervalMs)
        {
            return;
        }

        std::string payload = createPayload(tsl, bme, mlx, gps);

        Logger::debug(TAG, "Publishing payload (%d bytes)", payload.length());

        if (mqttClient->publish(config.topic.c_str(), payload.c_str()))
        {
            Logger::info(TAG, "Published sensor data (%d bytes)", payload.length());
            lastPublish = now;
        }
        else
        {
            Logger::error(TAG, "Failed to publish sensor data (payload: %d bytes, buffer: %d bytes)",
                          payload.length(), mqttClient->getBufferSize());
        }
    }

    void MQTTClient::updateConfig(const MQTTConfig &newConfig)
    {
        config = newConfig;

        if (config.enabled)
        {
            mqttClient->setServer(config.broker.c_str(), config.port);
            if (!mqttClient->connected())
            {
                connect();
            }
        }
        else
        {
            mqttClient->disconnect();
        }
    }

    void MQTTClient::connect()
    {
        if (!config.enabled)
            return;

        Logger::info(TAG, "Connecting to MQTT broker: %s:%d", config.broker.c_str(), config.port);

        bool connected = false;
        if (!config.username.empty())
        {
            connected = mqttClient->connect(
                config.topic.c_str(),
                config.username.c_str(),
                config.password.c_str());
        }
        else
        {
            connected = mqttClient->connect(config.topic.c_str());
        }

        if (connected)
        {
            Logger::info(TAG, "Connected to MQTT broker");
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

    std::string MQTTClient::createPayload(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps)
    {
        StaticJsonDocument<1024> doc; // Increased from 768 to accommodate GPS data

        // Timestamp
        doc["timestamp"] = millis();

        // TSL2591 data
        const auto &tslReading = tsl.getReading();
        JsonObject light = doc.createNestedObject("light");
        light["lux"] = tslReading.lux;
        light["visible"] = tslReading.visible;
        light["infrared"] = tslReading.infrared;

        // Sky quality
        SkyQualityMetrics sqm = SkyQuality::calculate(tslReading.lux);
        JsonObject sky = doc.createNestedObject("sky");
        sky["sqm"] = sqm.sqm;
        sky["nelm"] = sqm.nelm;
        sky["bortle"] = sqm.bortle;

        // BME280 data
        const auto &bmeReading = bme.getReading();
        JsonObject env = doc.createNestedObject("environment");
        env["temperature"] = bmeReading.temperature;
        env["humidity"] = bmeReading.humidity;
        env["pressure"] = bmeReading.pressure;

        // MLX90614 IR temperature data and cloud detection
        const auto &mlxReading = mlx.getReading();
        if (mlxReading.status == SensorStatus::OK)
        {
            JsonObject ir = doc.createNestedObject("infrared");
            ir["skyTemp"] = mlxReading.objectTemp;
            ir["ambientTemp"] = mlxReading.ambientTemp;

            // Cloud detection
            CloudMetrics cloudMetrics = CloudDetection::calculate(
                mlxReading.objectTemp,
                mlxReading.ambientTemp,
                bmeReading.humidity);

            JsonObject clouds = doc.createNestedObject("clouds");
            clouds["temperatureDelta"] = cloudMetrics.temperatureDelta;
            clouds["correctedDelta"] = cloudMetrics.correctedDelta;
            clouds["coverPercent"] = cloudMetrics.cloudCoverPercent;
            clouds["condition"] = static_cast<int>(cloudMetrics.condition);
            clouds["description"] = cloudMetrics.description;
        }
        else
        {
            // MLX sensor not available - skip IR and cloud data
            Logger::debug("MQTT", "MLX sensor not available, skipping IR/cloud data");
        }

        // GPS data
        const auto &gpsReading = gps.getReading();
        if (gpsReading.status == SensorStatus::OK && gpsReading.hasFix)
        {
            JsonObject location = doc.createNestedObject("location");
            location["latitude"] = gpsReading.latitude;
            location["longitude"] = gpsReading.longitude;
            location["altitude"] = gpsReading.altitude;
            location["satellites"] = gpsReading.satellites;
            location["hdop"] = gpsReading.hdop / 100.0; // Convert to actual value
        }

        std::string json;
        serializeJson(doc, json);
        return json;
    }

} // namespace SQM
