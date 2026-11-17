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
          recovery(config.reconnectDelayMs, config.maxReconnectDelayMs)
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
        // Arduino 3.x starts IPv6 (link-local, then SLAAC addresses from
        // router advertisements) for the station only if asked before connecting.
        WiFi.enableIPv6(ipv6Wanted);
        WiFi.onEvent(onWiFiEvent);
        recovery.begin(millis());

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
        const bool connected = isConnected();
        if (connected)
        {
            if (stationConnectedAt == 0)
                stationConnectedAt = millis() | 1;
            startMdns();
        }
        else
        {
            stationConnectedAt = 0;
        }

        WifiRecovery::State state;
        state.now = millis();
        state.hasCredentials = !config.ssid.empty();
        state.connected = connected;
        state.hotspotOpen = apMode;
        state.hotspotInUse = apMode && WiFi.softAPgetStationNum() > 0;
        state.busy = retryPaused || updateRunning;
        state.autoReconnect = config.autoReconnect;
        act(recovery.next(state));
    }

    // Carries out what WifiRecovery decided (specs/014-wifi-setup).
    void WiFiManager::act(WifiRecovery::Action action)
    {
        const unsigned long downSeconds = recovery.downForMs(millis()) / 1000;
        switch (action)
        {
        case WifiRecovery::Action::None:
            break;
        case WifiRecovery::Action::Retry:
            Logger::info(TAG, "Attempting to reconnect to WiFi (off %lu s)...", downSeconds);
            connectToWiFi();
            break;
        case WifiRecovery::Action::ResetStack:
            Logger::warn(TAG, "WiFi not joined for %lu s; restarting the WiFi stack", downSeconds);
            resetStack();
            break;
        case WifiRecovery::Action::OpenHotspot:
            if (config.ssid.empty())
                Logger::warn(TAG, "No saved network, opening the setup hotspot");
            else
                Logger::warn(TAG, "Saved network not joined since boot, opening the setup hotspot too");
            startCaptivePortal();
            break;
        case WifiRecovery::Action::CloseHotspot:
            Logger::warn(
                TAG,
                "Setup hotspot unused for %lu s; closing it to retry the saved network alone",
                static_cast<unsigned long>(WifiRecovery::HOTSPOT_IDLE_CLOSE_MS / 1000));
            stopCaptivePortal();
            resetStack();
            break;
        case WifiRecovery::Action::Restart:
            Logger::error(TAG, "WiFi not joined for %lu s; restarting the device to recover", downSeconds);
            restartDue = true;
            break;
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
        recovery.credentialsChanged(millis()); // fresh credentials get a fresh backoff
        connectToWiFi();

        return true;
    }

    void WiFiManager::connectToWiFi()
    {
        WiFi.begin(config.ssid.c_str(), config.password.c_str());
    }

    // WiFi.begin() alone has been seen not to rejoin after the access point
    // came back; switching WiFi off and on again starts the driver afresh
    // (as a restart does). Only with the hotspot closed.
    void WiFiManager::resetStack()
    {
        if (mdnsStarted)
        {
            MDNS.end(); // its interface goes; started again once joined
            mdnsStarted = false;
        }
        // Station off - the whole driver, with the hotspot closed. The saved
        // network isn't erased (it lives in our config anyway).
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        // The station interface was recreated: IPv6 must be asked for again.
        WiFi.enableIPv6(ipv6Wanted);
        connectToWiFi();
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
