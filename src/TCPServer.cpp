#include "TCPServer.h"
#include "Logger.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "calculations/CloudDetection.h"
#include "calculations/SkyQuality.h"
#include "version.h"
#include <math.h>

namespace SQM
{
    TCPServer::TCPServer(uint16_t port)
        : port(port), server(nullptr), tslSensor(nullptr), bmeSensor(nullptr), mlxSensor(nullptr), gpsSensor(nullptr)
    {
    }

    TCPServer::~TCPServer()
    {
        if (server)
        {
            delete server;
        }
    }

    void TCPServer::begin()
    {
        if (!WiFi.isConnected())
        {
            Logger::warn(TAG, "WiFi not connected, delaying TCP server start");
            return;
        }

        Logger::info(TAG, "Starting TCP server on port %d", port);
        server = new WiFiServer(port);
        server->begin();
        server->setNoDelay(true);
        Logger::info(TAG, "TCP server started on port %d", port);
    }

    void TCPServer::setSensorReferences(TSL2591Sensor *tsl, BME280Sensor *bme, MLX90614Sensor *mlx, GPSSensor *gps)
    {
        tslSensor = tsl;
        bmeSensor = bme;
        mlxSensor = mlx;
        gpsSensor = gps;
    }

    void TCPServer::handle()
    {
        if (!server)
            return;

        // Check for new client
        if (!client || !client.connected())
        {
            client = server->available();
            if (client)
            {
                Logger::info(TAG, "Client connected from %s", client.remoteIP().toString().c_str());
            }
        }

        // Handle existing client
        if (client && client.connected())
        {
            handleClient();
        }
    }

    void TCPServer::handleClient()
    {
        while (client.available())
        {
            String command = client.readStringUntil('#');
            command.trim();

            if (command.length() > 0)
            {
                Logger::debug(TAG, "Received command: %s#", command.c_str());
                processCommand(command);
            }
        }
    }

    void TCPServer::processCommand(const String &command)
    {
        String response;

        // Commands from ASCOM driver
        if (command == ":003")
            response = handleSkyQuality();
        else if (command == ":004")
            response = handleSkyBrightness();
        else if (command == ":008")
            response = handleFirmwareVersion();
        else if (command == ":009")
            response = handleFirmwareName();
        else if (command == ":028")
            response = handleHumidity();
        else if (command == ":029")
            response = handlePressure();
        else if (command == ":030")
            response = handleAmbientTemp();
        else if (command == ":031")
            response = handleDewpoint();
        else if (command == ":035")
            response = handleSkyTemp();
        else if (command == ":038")
            response = handleCloudCover();
        else if (command == ":051")
            response = handleRainRate();
        else if (command == ":054")
            response = handleWindSpeed();
        else if (command == ":055")
            response = handleWindDirection();
        else if (command == ":059")
            response = handleWindGust();
        else
        {
            Logger::warn(TAG, "Unknown command: %s", command.c_str());
            response = "0.0";
        }

        sendResponse(response);
    }

    void TCPServer::sendResponse(const String &data)
    {
        // Format: <data>#
        String response = "<" + data + "#";
        client.print(response);
        client.flush(); // Ensure data is sent immediately
        Logger::debug(TAG, "Sent: %s", response.c_str());
    }

    String TCPServer::handleSkyQuality()
    {
        if (!tslSensor || !tslSensor->isInitialized())
            return "0.0";

        TSL2591Reading reading = tslSensor->getReading();
        if (reading.status != SensorStatus::OK)
            return "0.0";

        float sqm = SkyQuality::luxToSQM(reading.lux);
        return String(sqm, 2);
    }

    String TCPServer::handleSkyBrightness()
    {
        if (!tslSensor || !tslSensor->isInitialized())
            return "0.0";

        TSL2591Reading reading = tslSensor->getReading();
        if (reading.status != SensorStatus::OK)
            return "0.0";

        return String(reading.lux, 2);
    }

    String TCPServer::handleFirmwareVersion()
    {
        return String(FIRMWARE_VERSION);
    }

    String TCPServer::handleFirmwareName()
    {
        return String("SQMeter");
    }

    String TCPServer::handleHumidity()
    {
        if (!bmeSensor || !bmeSensor->isInitialized())
            return "0.0";

        BME280Reading reading = bmeSensor->getReading();
        if (reading.status != SensorStatus::OK || !reading.isValid())
            return "0.0";

        return String(reading.humidity, 2);
    }

    String TCPServer::handlePressure()
    {
        if (!bmeSensor || !bmeSensor->isInitialized())
            return "0.0";

        BME280Reading reading = bmeSensor->getReading();
        if (reading.status != SensorStatus::OK || !reading.isValid())
            return "0.0";

        return String(reading.pressure, 2);
    }

    String TCPServer::handleAmbientTemp()
    {
        if (!bmeSensor || !bmeSensor->isInitialized())
            return "0.0";

        BME280Reading reading = bmeSensor->getReading();
        if (reading.status != SensorStatus::OK || !reading.isValid())
            return "0.0";

        return String(reading.temperature, 2);
    }

    String TCPServer::handleDewpoint()
    {
        if (!bmeSensor || !bmeSensor->isInitialized())
            return "0.0";

        BME280Reading reading = bmeSensor->getReading();
        if (reading.status != SensorStatus::OK || !reading.isValid())
            return "0.0";

        return String(reading.dewpoint, 2);
    }

    String TCPServer::handleSkyTemp()
    {
        if (!mlxSensor || !mlxSensor->isInitialized())
            return "0.0";

        MLX90614Reading reading = mlxSensor->getReading();
        if (reading.status != SensorStatus::OK)
            return "0.0";

        return String(reading.objectTemp, 2);
    }

    String TCPServer::handleCloudCover()
    {
        if (!mlxSensor || !mlxSensor->isInitialized() || !bmeSensor || !bmeSensor->isInitialized())
            return "0.0";

        MLX90614Reading mlxReading = mlxSensor->getReading();
        BME280Reading bmeReading = bmeSensor->getReading();

        if (mlxReading.status != SensorStatus::OK || bmeReading.status != SensorStatus::OK || !bmeReading.isValid())
            return "0.0";

        CloudMetrics metrics = CloudDetection::calculate(mlxReading.objectTemp, mlxReading.ambientTemp, bmeReading.humidity);
        return String(metrics.cloudCoverPercent, 1);
    }

    String TCPServer::handleRainRate()
    {
        // TODO: Implement when rain sensor added
        return "0.0";
    }

    String TCPServer::handleWindSpeed()
    {
        // TODO: Implement when wind sensor added
        return "0.0";
    }

    String TCPServer::handleWindDirection()
    {
        // TODO: Implement when wind sensor added
        return "0.0";
    }

    String TCPServer::handleWindGust()
    {
        // TODO: Implement when wind sensor added
        return "0.0";
    }

} // namespace SQM
