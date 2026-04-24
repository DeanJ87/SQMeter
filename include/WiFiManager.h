#pragma once

#include "Config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <functional>
#include <optional>

namespace SQM
{

    class WiFiManager
    {
    public:
        using OnConnectedCallback = std::function<void()>;
        using OnDisconnectedCallback = std::function<void()>;

        explicit WiFiManager(const WiFiConfig &config);
        ~WiFiManager();

        // Delete copy operations
        WiFiManager(const WiFiManager &) = delete;
        WiFiManager &operator=(const WiFiManager &) = delete;

        void begin();
        void handle();

        bool isConnected() const;
        bool isInAPMode() const;
        std::string getIPAddress() const;
        std::string getMACAddress() const;
        int32_t getRSSI() const;

        void setOnConnected(OnConnectedCallback callback);
        void setOnDisconnected(OnDisconnectedCallback callback);

        // Captive portal for WiFi setup
        void startCaptivePortal();
        void stopCaptivePortal();
        bool updateCredentials(const std::string &ssid, const std::string &password);

    private:
        static constexpr const char *TAG = "WiFiManager";
        static constexpr const char *AP_SSID = "SQM-Setup";
        static constexpr const uint8_t DNS_PORT = 53;

        WiFiConfig config;
        bool apMode;
        uint32_t lastReconnectAttempt;
        uint32_t currentReconnectDelay;

        std::optional<DNSServer> dnsServer;
        OnConnectedCallback onConnectedCallback;
        OnDisconnectedCallback onDisconnectedCallback;

        void connectToWiFi();
        void handleReconnect();
        void startAPMode();
        void stopAPMode();

        static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    };

} // namespace SQM
