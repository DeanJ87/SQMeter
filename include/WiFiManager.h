#pragma once

#include "Config.h"
#include <WiFi.h>
#include <AsyncUDP.h>
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

        // While the setup hotspot is up, the saved network is retried in the
        // background; pause that while the setup screen tries new credentials.
        void setRetryPaused(bool paused) { retryPaused = paused; }
        // True once the station has been connected for `ms` while the hotspot
        // is still up - time to restart into normal operation.
        bool connectedFromHotspotFor(uint32_t ms) const;

        // Starts mDNS (<hostname>.local + HTTP service) if enabled; idempotent.
        void startMdns();
        bool isMdnsRunning() const { return mdnsStarted; }

    private:
        static constexpr const char *TAG = "WiFiManager";
        static constexpr const char *AP_SSID = "SQM-Setup";

        WiFiConfig config;
        bool apMode;
        uint32_t lastReconnectAttempt;
        uint32_t currentReconnectDelay;
        bool retryPaused = false;
        bool mdnsStarted = false;
        uint32_t stationConnectedAt = 0;

        // Answers DNS from the network task, so phones get replies at once
        // even while the main loop is busy (a light-sensor read takes ~0.7 s).
        std::optional<AsyncUDP> dnsServer;
        OnConnectedCallback onConnectedCallback;
        OnDisconnectedCallback onDisconnectedCallback;

        void connectToWiFi();
        void handleReconnect();
        void startAPMode();
        void stopAPMode();

        static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    };

} // namespace SQM
