#include "MQTTClient.h"
#include "Logger.h"
#include "calculations/SkyQuality.h"
#include "calculations/CloudDetection.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

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
        mqttClient->setBufferSize(3072); // Fits cloud and RG-15 diagnostics payloads
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
        status.availabilityTopic = getAvailabilityTopic();
        status.clientId = buildClientId();
        return status;
    }

    void MQTTClient::publishSensorData(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps, const RG15Sensor &rg15)
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

        std::string payload = createPayload(tsl, bme, mlx, gps, rg15);

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
                clientId.c_str(),
                config.username.c_str(),
                config.password.c_str(),
                availabilityTopic.c_str(),
                1,
                true,
                "offline");
        }
        else
        {
            connected = mqttClient->connect(
                clientId.c_str(),
                availabilityTopic.c_str(),
                1,
                true,
                "offline");
        }

        if (connected)
        {
            Logger::info(TAG, "Connected to MQTT broker as %s", clientId.c_str());
            publishAvailability(true);
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

    std::string MQTTClient::getAvailabilityTopic() const
    {
        return config.topic + "/availability";
    }

    std::string MQTTClient::createPayload(const TSL2591Sensor &tsl, const BME280Sensor &bme, const MLX90614Sensor &mlx, const GPSSensor &gps, const RG15Sensor &rg15)
    {
        StaticJsonDocument<3072> doc;

        // Timestamp
        const time_t epochSeconds = time(nullptr);
        const bool timeValid = epochSeconds >= 1704067200; // 2024-01-01T00:00:00Z
        doc["timestamp"] = timeValid ? static_cast<int64_t>(epochSeconds) : static_cast<int64_t>(millis());
        doc["timeValid"] = timeValid;

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
            location["hdop"] = gpsReading.hdop / 100.0;
        }

        // RG-15 rain sensor data and diagnostics
        const auto rg15Reading = rg15.copyReading();
        const auto rg15Diag = rg15.getDiagnostics();
        JsonObject rain = doc.createNestedObject("rain");
        rain["enabled"] = rg15Diag.enabled;
        rain["sensor"] = "hydreon_rg15";
        rain["initialized"] = rg15Diag.uartOpened;
        rain["online"] = rg15Reading.online;
        rain["stale"] = rg15Reading.stale;
        rain["status"] = static_cast<int>(rg15Reading.status);
        rain["timestamp"] = rg15Reading.timestamp;
        rain["ageMs"] = rg15Reading.ageMs;
        rain["isRaining"] = rg15Reading.isRaining;
        rain["acc"] = rg15Reading.acc;
        rain["eventAcc"] = rg15Reading.eventAcc;
        rain["totalAcc"] = rg15Reading.totalAcc;
        rain["rInt"] = rg15Reading.rInt;
        rain["accumulation_since_last_read"] = rg15Reading.acc;
        rain["event_accumulation"] = rg15Reading.eventAcc;
        rain["total_accumulation"] = rg15Reading.totalAcc;
        rain["rain_intensity"] = rg15Reading.rInt;
        rain["lensBad"] = rg15Reading.lensBad;
        rain["emSat"] = rg15Reading.emSat;
        rain["units"] = rg15Diag.units.c_str();

        JsonObject uart = rain.createNestedObject("uart");
        uart["configured"] = rg15Diag.configured;
        uart["opened"] = rg15Diag.uartOpened;
        uart["rx_pin"] = rg15Diag.rxPin;
        uart["tx_pin"] = rg15Diag.txPin;
        uart["baud_rate"] = rg15Diag.baudRate;
        uart["uart_port"] = rg15Diag.uartPort;
        uart["mode"] = rg15Diag.mode.c_str();
        uart["resolution"] = rg15Diag.resolution.c_str();
        uart["units"] = rg15Diag.units.c_str();
        uart["debug_uart"] = rg15Diag.debugUart;
        uart["timeouts"] = rg15Diag.timeouts;
        uart["parse_errors"] = rg15Diag.parseErrors;
        uart["successful_reads"] = rg15Diag.successfulReads;
        if (rg15Diag.lastHealthCheckMs != 0)
        {
            uart["last_health_check_ms"] = rg15Diag.lastHealthCheckMs;
            uart["last_health_check_age_ms"] = millis() - rg15Diag.lastHealthCheckMs;
        }
        if (rg15Diag.lastStatusLine)
            uart["last_status_line"] = rg15Diag.lastStatusLine->c_str();
        if (rg15Diag.softwareVersion)
            uart["software_version"] = rg15Diag.softwareVersion->c_str();
        if (rg15Diag.softwareBuildDate)
            uart["software_build_date"] = rg15Diag.softwareBuildDate->c_str();
        if (rg15Diag.resetReason)
            uart["reset_reason"] = rg15Diag.resetReason->c_str();
        if (rg15Diag.powerOnDays)
            uart["power_on_days"] = *rg15Diag.powerOnDays;
        if (rg15Diag.emitter1)
            uart["emitter_1"] = *rg15Diag.emitter1;
        if (rg15Diag.emitter2)
            uart["emitter_2"] = *rg15Diag.emitter2;
        if (rg15Diag.emitterTotal)
            uart["emitter_total"] = *rg15Diag.emitterTotal;
        if (rg15Diag.lastCommand)
            uart["last_command"] = rg15Diag.lastCommand->c_str();
        if (rg15Diag.lastAck)
            uart["last_ack"] = rg15Diag.lastAck->c_str();
        if (rg15Diag.lastRawResponse)
            uart["last_raw_response"] = rg15Diag.lastRawResponse->c_str();
        if (rg15Diag.lastError)
            uart["last_error"] = rg15Diag.lastError->c_str();
        if (rg15Diag.lastSuccessfulReadMs != 0)
        {
            uart["last_successful_read_ms"] = rg15Diag.lastSuccessfulReadMs;
            uart["last_successful_read_age_ms"] = millis() - rg15Diag.lastSuccessfulReadMs;
        }

        std::string json;
        serializeJson(doc, json);
        return json;
    }

} // namespace SQM
