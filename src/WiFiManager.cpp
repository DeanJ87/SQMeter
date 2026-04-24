#include "WiFiManager.h"
#include "Logger.h"
#include <WiFi.h>

namespace SQM
{

    WiFiManager::WiFiManager(const WiFiConfig &config)
        : config(config), apMode(false), lastReconnectAttempt(0), currentReconnectDelay(config.reconnectDelayMs)
    {
    }

    WiFiManager::~WiFiManager()
    {
        stopAPMode();
        WiFi.disconnect(true);
    }

    void WiFiManager::begin()
    {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(config.hostname.c_str());
        WiFi.onEvent(onWiFiEvent);

        if (!config.ssid.empty())
        {
            Logger::info(TAG, "Connecting to WiFi: %s", config.ssid.c_str());
            connectToWiFi();
        }
        else
        {
            Logger::warn(TAG, "No WiFi credentials configured, starting captive portal");
            startCaptivePortal();
        }
    }

    void WiFiManager::handle()
    {
        if (apMode && dnsServer)
        {
            dnsServer->processNextRequest();
        }

        if (!apMode && config.autoReconnect && !isConnected())
        {
            handleReconnect();
        }
    }

    bool WiFiManager::isConnected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

    bool WiFiManager::isInAPMode() const
    {
        return apMode;
    }

    std::string WiFiManager::getIPAddress() const
    {
        if (apMode)
        {
            return WiFi.softAPIP().toString().c_str();
        }
        return WiFi.localIP().toString().c_str();
    }

    std::string WiFiManager::getMACAddress() const
    {
        return WiFi.macAddress().c_str();
    }

    int32_t WiFiManager::getRSSI() const
    {
        return WiFi.RSSI();
    }

    void WiFiManager::setOnConnected(OnConnectedCallback callback)
    {
        onConnectedCallback = callback;
    }

    void WiFiManager::setOnDisconnected(OnDisconnectedCallback callback)
    {
        onDisconnectedCallback = callback;
    }

    void WiFiManager::startCaptivePortal()
    {
        Logger::info(TAG, "Starting captive portal: %s", AP_SSID);

        apMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID);

        dnsServer.emplace();
        dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());

        Logger::info(TAG, "Captive portal started at %s", getIPAddress().c_str());
    }

    void WiFiManager::stopCaptivePortal()
    {
        if (!apMode)
            return;

        Logger::info(TAG, "Stopping captive portal");

        if (dnsServer)
        {
            dnsServer->stop();
            dnsServer.reset();
        }

        WiFi.softAPdisconnect(true);
        apMode = false;
    }

    bool WiFiManager::updateCredentials(const std::string &ssid, const std::string &password)
    {
        Logger::info(TAG, "Updating WiFi credentials: %s", ssid.c_str());

        config.ssid = ssid;
        config.password = password;

        stopCaptivePortal();
        WiFi.mode(WIFI_STA);
        connectToWiFi();

        return true;
    }

    void WiFiManager::connectToWiFi()
    {
        WiFi.begin(config.ssid.c_str(), config.password.c_str());
        lastReconnectAttempt = millis();
        currentReconnectDelay = config.reconnectDelayMs;
    }

    void WiFiManager::handleReconnect()
    {
        const uint32_t now = millis();

        if (now - lastReconnectAttempt >= currentReconnectDelay)
        {
            Logger::info(TAG, "Attempting to reconnect to WiFi...");
            connectToWiFi();

            // Exponential backoff
            currentReconnectDelay = std::min(
                currentReconnectDelay * 2,
                config.maxReconnectDelayMs);
        }
    }

    void WiFiManager::startAPMode()
    {
        startCaptivePortal();
    }

    void WiFiManager::stopAPMode()
    {
        stopCaptivePortal();
    }

    void WiFiManager::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        switch (event)
        {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Logger::info(TAG, "WiFi connected");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Logger::info(TAG, "Got IP address: %s", WiFi.localIP().toString().c_str());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Logger::warn(TAG, "WiFi disconnected");
            break;

        default:
            break;
        }
    }

} // namespace SQM
