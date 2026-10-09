#include "WiFiManager.h"
#include "Logger.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "CaptiveDns.h"
#include <esp_netif.h>

namespace SQM
{
    bool WiFiManager::ipv6Wanted = false;

    WiFiManager::WiFiManager(const WiFiConfig &config)
        : config(config),
          apMode(false),
          lastReconnectAttempt(0),
          currentReconnectDelay(config.reconnectDelayMs)
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
        ipv6Wanted = config.ipv6;
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
        if (isConnected())
        {
            if (stationConnectedAt == 0)
                stationConnectedAt = millis() | 1;
            startMdns();
        }
        else
        {
            stationConnectedAt = 0;
        }

        if (apMode)
        {
            // Keep trying the saved network (e.g. the router came up after the
            // device after a power cut) so the device doesn't sit in setup mode.
            if (!config.ssid.empty() && !retryPaused && !isConnected())
                handleReconnect();
            return;
        }

        if (!config.autoReconnect)
        {
            return;
        }

        if (isConnected())
        {
            // Reset backoff once healthy, so the next outage starts from the
            // short base delay again instead of resuming at whatever the
            // previous outage had escalated to.
            currentReconnectDelay = config.reconnectDelayMs;
        }
        else
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

    bool WiFiManager::connectedFromHotspotFor(uint32_t ms) const
    {
        return apMode && stationConnectedAt != 0 && isConnected() && millis() - stationConnectedAt >= ms;
    }

    void WiFiManager::startMdns()
    {
        if (mdnsStarted || !config.mdns || !isConnected())
            return;
        if (!MDNS.begin(config.hostname.c_str()))
        {
            Logger::warn(TAG, "mDNS failed to start");
            mdnsStarted = true; // don't retry every loop
            return;
        }
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
        Logger::info(TAG, "mDNS: http://%s.local", config.hostname.c_str());
    }

    void WiFiManager::startCaptivePortal()
    {
        Logger::info(TAG, "Starting captive portal: %s", AP_SSID);

        apMode = true;
        // AP+STA so the setup screen can scan and try networks, and the saved
        // network keeps being retried.
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID);
        currentReconnectDelay = std::max<uint32_t>(config.reconnectDelayMs, 30000);
        lastReconnectAttempt = millis();

        dnsServer.emplace();
        if (dnsServer->listen(CaptiveDns::PORT))
        {
            dnsServer->onPacket(
                [](AsyncUDPPacket &packet)
                {
                    const IPAddress ip = WiFi.softAPIP();
                    const uint8_t address[4] = {ip[0], ip[1], ip[2], ip[3]};
                    std::vector<uint8_t> reply;
                    if (CaptiveDns::buildResponse(packet.data(), packet.length(), address, reply))
                        packet.write(reply.data(), reply.size());
                });
        }
        else
        {
            Logger::error(TAG, "Captive DNS failed to start");
        }

        Logger::info(TAG, "Captive portal started at %s", getIPAddress().c_str());
    }

    void WiFiManager::stopCaptivePortal()
    {
        if (!apMode)
            return;

        Logger::info(TAG, "Stopping captive portal");

        if (dnsServer)
        {
            dnsServer->close();
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
        currentReconnectDelay = config.reconnectDelayMs; // fresh credentials get a fresh backoff
        connectToWiFi();

        return true;
    }

    void WiFiManager::connectToWiFi()
    {
        WiFi.begin(config.ssid.c_str(), config.password.c_str());
        lastReconnectAttempt = millis();
    }

    void WiFiManager::handleReconnect()
    {
        const uint32_t now = millis();

        if (now - lastReconnectAttempt >= currentReconnectDelay)
        {
            Logger::info(TAG, "Attempting to reconnect to WiFi...");
            connectToWiFi();

            // Exponential backoff
            currentReconnectDelay = std::min(currentReconnectDelay * 2, config.maxReconnectDelayMs);
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

    std::vector<Net::Ipv6> WiFiManager::ipv6Addresses()
    {
        std::vector<Net::Ipv6> addresses;
        esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!ipv6Wanted || station == nullptr || !WiFi.isConnected())
            return addresses;
        esp_ip6_addr_t found[LWIP_IPV6_NUM_ADDRESSES];
        const int count = esp_netif_get_all_ip6(station, found);
        for (int i = 0; i < count; ++i)
        {
            Net::Ipv6 address{};
            memcpy(address.data(), found[i].addr, address.size());
            addresses.push_back(address);
        }
        return addresses;
    }

    void WiFiManager::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        switch (event)
        {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Logger::info(TAG, "WiFi connected");
            // A link-local address starts IPv6; router advertisements then
            // add global/unique-local ones by themselves (SLAAC).
            if (ipv6Wanted && !WiFi.enableIpV6())
                Logger::warn(TAG, "IPv6 failed to start");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        {
            Net::Ipv6 address{};
            memcpy(address.data(), info.got_ip6.ip6_info.ip.addr, address.size());
            Logger::info(TAG, "Got IPv6 address: %s (%s)", Net::formatIpv6(address).c_str(), Net::scopeName(Net::scopeOf(address)));
            break;
        }

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
