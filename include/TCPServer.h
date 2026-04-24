#pragma once

#include <WiFi.h>
#include <memory>

namespace SQM
{
    // Forward declarations
    class TSL2591Sensor;
    class BME280Sensor;
    class MLX90614Sensor;
    class GPSSensor;

    class TCPServer
    {
    public:
        TCPServer(uint16_t port = 2020);
        ~TCPServer();

        void begin();
        void handle();
        void setSensorReferences(TSL2591Sensor *tsl, BME280Sensor *bme, MLX90614Sensor *mlx, GPSSensor *gps);

    private:
        static constexpr const char *TAG = "TCPServer";

        uint16_t port;
        WiFiServer *server;
        WiFiClient client;

        // Sensor references
        TSL2591Sensor *tslSensor;
        BME280Sensor *bmeSensor;
        MLX90614Sensor *mlxSensor;
        GPSSensor *gpsSensor;

        void handleClient();
        void processCommand(const String &command);
        void sendResponse(const String &data);

        // Command handlers
        String handleSkyQuality();      // :003#
        String handleSkyBrightness();   // :004#
        String handleFirmwareVersion(); // :008#
        String handleFirmwareName();    // :009#
        String handleHumidity();        // :028#
        String handlePressure();        // :029#
        String handleAmbientTemp();     // :030#
        String handleDewpoint();        // :031#
        String handleSkyTemp();         // :035#
        String handleCloudCover();      // :038#
        String handleRainRate();        // :051#
        String handleWindSpeed();       // :054#
        String handleWindDirection();   // :055#
        String handleWindGust();        // :059#
    };

} // namespace SQM
