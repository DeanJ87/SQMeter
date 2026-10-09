#pragma once

#include "Config.h"
#include "NetAddress.h"
#include <WiFi.h>
#include <AsyncUDP.h>
#include <functional>
#include <optional>
#include <vector>

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

        // IPv6 on the joined network (spec 015): on when the setting is, from
        // the next connection. Addresses are the station's valid ones
        // (link-local, then SLAAC); empty while IPv6 is off or not connected.
        bool isIpv6Enabled() const { return config.ipv6; }
        // The setting the device started with (changes apply at restart).
        static bool ipv6Running() { return ipv6Wanted; }
        static std::vector<Net::Ipv6> ipv6Addresses();

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
        // Boot fallback (spec 014): when trying started, and whether the saved
        // network has been joined since boot.
        uint32_t startedTryingAt = 0;
        bool connectedSinceBoot = false;

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
        // Read by the static event handler (it runs on the WiFi event task).
        static bool ipv6Wanted;
    };

} // namespace SQM
