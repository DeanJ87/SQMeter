#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "Logger.h"
#include "Config.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "MQTTClient.h"
#include "TimeManager.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/RG15Sensor.h"

using namespace SQM;

// Global objects
static Config config;
static std::unique_ptr<WiFiManager> wifiManager;
static std::unique_ptr<TSL2591Sensor> tslSensor;
static std::unique_ptr<BME280Sensor> bmeSensor;
static std::unique_ptr<MLX90614Sensor> mlxSensor;
static std::unique_ptr<GPSSensor> gpsSensor;
static std::unique_ptr<RG15Sensor> rg15Sensor;
static std::unique_ptr<TimeManager> timeManager;
static std::unique_ptr<WebServer> webServer;
static std::unique_ptr<MQTTClient> mqttClient;
static bool arduinoOTAEnabled = false;
RTC_DATA_ATTR uint32_t bootCount = 0;

// Timing
static uint32_t lastSensorUpdate = 0;

// Callbacks for WebServer
const Config &getConfigCallback()
{
    return config;
}

bool saveConfigCallback(const Config &newConfig)
{
    config = newConfig;
    bool saved = config.save();

    if (saved)
    {
        // Update MQTT config if changed
        if (mqttClient)
        {
            mqttClient->updateConfig(config.mqtt);
        }

        // Update time manager config if changed
        if (timeManager)
        {
            timeManager->updateConfig(config.ntp, config.gps,
                                      config.primaryTimeSource, config.secondaryTimeSource);
        }

        if (!config.ota.enabled || config.ota.password.empty())
        {
            arduinoOTAEnabled = false;
        }
        else if (!arduinoOTAEnabled)
        {
            Logger::warn("Main", "ArduinoOTA config saved; restart required to start command-line OTA");
        }

        // Reconfigure RG-15 in-place so WebServer's reference stays valid
        if (rg15Sensor)
        {
            if (config.rain.enabled)
            {
                rg15Sensor->reconfigure(
                    config.rain.rxPin, config.rain.txPin, config.rain.baudRate,
                    config.rain.mode, config.rain.resolution, config.rain.units,
                    config.rain.debugUart, config.rain.pollIntervalMs,
                    config.rain.rainClearDelayMs, config.rain.dailyResetEnabled,
                    config.rain.dailyResetHour, config.rain.dailyResetMinute);
            }
            else
            {
                rg15Sensor->stop();
            }
        }
    }

    return saved;
}

void setupWatchdog()
{
    Logger::info("Main", "Configuring watchdog timer (30s)");
    esp_task_wdt_init(30, true); // 30 second timeout
    esp_task_wdt_add(NULL);
}

void setupI2C()
{
    Logger::info("Main", "Initializing I2C (SDA:%d, SCL:%d, %dHz)",
                 config.sensor.i2cSDA, config.sensor.i2cSCL, config.sensor.i2cFrequency);

    Wire.begin(config.sensor.i2cSDA, config.sensor.i2cSCL);
    Wire.setClock(config.sensor.i2cFrequency);
}

void setupSensors()
{
    Logger::info("Main", "Initializing sensors");

    tslSensor = std::make_unique<TSL2591Sensor>();
    bmeSensor = std::make_unique<BME280Sensor>();
    mlxSensor = std::make_unique<MLX90614Sensor>();

    if (!tslSensor->begin())
    {
        Logger::error("Main", "Failed to initialize TSL2591");
    }

    if (!bmeSensor->begin())
    {
        Logger::warn("Main", "BME280 not detected (optional sensor)");
    }

    if (!mlxSensor->begin())
    {
        Logger::warn("Main", "MLX90614 not detected (optional sensor)");
    }

    // Initialize GPS (UART, not I2C)
    // Configuration-driven initialization
    if (config.gps.enabled)
    {
        gpsSensor = std::make_unique<GPSSensor>(config.gps.rxPin, config.gps.txPin, config.gps.baudRate);
        if (!gpsSensor->begin())
        {
            Logger::warn("Main", "GPS initialization failed");
        }
    }
    else
    {
        // Create disabled GPS sensor for API consistency
        gpsSensor = std::make_unique<GPSSensor>();
        Logger::info("Main", "GPS disabled in configuration");
    }

    // Initialize RG-15 rain sensor (UART, not I2C)
    if (config.rain.enabled)
    {
        rg15Sensor = std::make_unique<RG15Sensor>(
            config.rain.rxPin, config.rain.txPin, config.rain.baudRate,
            config.rain.mode, config.rain.resolution, config.rain.units,
            true, config.rain.debugUart, config.rain.pollIntervalMs,
            config.rain.rainClearDelayMs, config.rain.dailyResetEnabled,
            config.rain.dailyResetHour, config.rain.dailyResetMinute);
        if (!rg15Sensor->begin())
        {
            Logger::warn("Main", "RG-15 initialization failed");
        }
    }
    else
    {
        rg15Sensor = std::make_unique<RG15Sensor>();
        Logger::info("Main", "RG-15 rain sensor disabled in configuration");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    bootCount++;

    // Initialize logging
    Logger::init();
    Logger::info("Main", "=== SQMeter Starting ===");
    Logger::info("Main", "ESP32 Chip: %s Rev %d", ESP.getChipModel(), ESP.getChipRevision());
    Logger::info("Main", "Flash: %d bytes", ESP.getFlashChipSize());
    Logger::info("Main", "Free heap: %d bytes", ESP.getFreeHeap());

    // Setup watchdog
    setupWatchdog();

    // Load configuration
    auto configOpt = Config::load();
    if (configOpt)
    {
        config = *configOpt;
        Logger::info("Main", "Configuration loaded");
    }
    else
    {
        config = Config::createDefault();
        Logger::warn("Main", "Using default configuration");
    }

    // Mount LittleFS for serving web files
    if (!LittleFS.begin(false))
    {
        Logger::error("Main", "Failed to mount LittleFS; filesystem not formatted automatically");
    }
    else
    {
        Logger::info("Main", "LittleFS mounted successfully");
    }

    // Initialize I2C
    setupI2C();

    // Initialize sensors
    setupSensors();

    // Initialize WiFi
    wifiManager = std::make_unique<WiFiManager>(config.wifi);
    wifiManager->begin();

    // Wait for WiFi connection or start AP mode
    int attempts = 0;
    while (!wifiManager->isConnected() && !wifiManager->isInAPMode() && attempts < 20)
    {
        delay(500);
        attempts++;
    }

    if (!wifiManager->isConnected() && !wifiManager->isInAPMode())
    {
        Logger::warn("Main", "WiFi connection failed, starting captive portal");
        wifiManager->startCaptivePortal();
    }

    // Initialize ArduinoOTA for command-line firmware uploads only when configured securely.
    if (wifiManager->isConnected() && config.ota.enabled && !config.ota.password.empty())
    {
        ArduinoOTA.setHostname(config.wifi.hostname.c_str());
        ArduinoOTA.setPassword(config.ota.password.c_str());

        ArduinoOTA.onStart([]()
                           {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "firmware";
            } else { // U_SPIFFS
                type = "filesystem";
                LittleFS.end(); // Unmount filesystem
            }
            Logger::info("OTA", "Start updating %s", type.c_str()); });

        ArduinoOTA.onEnd([]()
                         { Logger::info("OTA", "Update complete"); });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              {
            static int lastPercent = -1;
            int percent = (progress / (total / 100));
            if (percent != lastPercent && percent % 10 == 0) {
                Logger::info("OTA", "Progress: %u%%", percent);
                lastPercent = percent;
            } });

        ArduinoOTA.onError([](ota_error_t error)
                           {
            Logger::error("OTA", "Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Logger::error("OTA", "Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Logger::error("OTA", "Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Logger::error("OTA", "Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Logger::error("OTA", "Receive Failed");
            else if (error == OTA_END_ERROR) Logger::error("OTA", "End Failed"); });

        ArduinoOTA.begin();
        arduinoOTAEnabled = true;
        Logger::info("Main", "ArduinoOTA enabled with password authentication");
    }
    else if (wifiManager->isConnected())
    {
        Logger::warn("Main", "ArduinoOTA disabled; configure ota.enabled and ota.password to enable command-line OTA");
    }

    // Initialize time manager with GPS/NTP priority
    timeManager = std::make_unique<TimeManager>(
        config.ntp,
        config.gps,
        config.primaryTimeSource,
        config.secondaryTimeSource,
        gpsSensor.get());
    if (wifiManager->isConnected())
    {
        timeManager->begin();
    }

    // Initialize MQTT
    mqttClient = std::make_unique<MQTTClient>(config.mqtt);
    mqttClient->begin();

    // Initialize web server
    webServer = std::make_unique<WebServer>(
        *tslSensor,
        *bmeSensor,
        *mlxSensor,
        *gpsSensor,
        *rg15Sensor,
        timeManager.get(),
        mqttClient.get(),
        getConfigCallback,
        saveConfigCallback);
    webServer->begin();
    webServer->refreshSensorSnapshot(lastSensorUpdate);

    Logger::info("Main", "=== Setup complete ===");
    Logger::info("Main", "IP Address: %s", wifiManager->getIPAddress().c_str());
    Logger::info("Main", "MAC Address: %s", wifiManager->getMACAddress().c_str());
}

void loop()
{
    // Feed watchdog
    esp_task_wdt_reset();

    // Handle WiFi
    wifiManager->handle();

    // Handle ArduinoOTA
    if (arduinoOTAEnabled)
    {
        ArduinoOTA.handle();
    }

    // Handle time manager
    if (timeManager)
    {
        timeManager->handle();
    }

    // Handle web server
    webServer->handle();

    // Handle MQTT
    if (mqttClient)
    {
        mqttClient->handle();
    }

    // Update sensors periodically
    const uint32_t now = millis();
    if (now - lastSensorUpdate >= config.sensor.readIntervalMs)
    {
        tslSensor->update();
        bmeSensor->update();
        mlxSensor->update();
        gpsSensor->update();
        rg15Sensor->update();
        lastSensorUpdate = now;
        webServer->refreshSensorSnapshot(lastSensorUpdate);

        // Publish to MQTT if enabled
        if (mqttClient && mqttClient->isEnabled())
        {
            mqttClient->publishSensorData(*tslSensor, *bmeSensor, *mlxSensor, *gpsSensor, *rg15Sensor);
        }
    }

    // Small delay to prevent tight loop
    delay(10);
}
