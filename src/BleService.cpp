#include "BleService.h"

#if SQM_ENABLE_BLE

#include "Logger.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <esp_coexist.h>

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "BLE";
        constexpr uint32_t SUMMARY_NOTIFY_INTERVAL_MS = 30000;
        constexpr uint32_t ADVERT_REFRESH_INTERVAL_MS = 60000;
        constexpr size_t MAX_ADVERTISED_NAME = 20;
        // Advertising interval in 0.625 ms units: 500-1000 ms. NimBLE's default
        // (20-40 ms) hogs the shared 2.4 GHz radio and starves WiFi (seconds
        // of ping latency, HTTP timeouts); a status beacon doesn't need it.
        constexpr uint16_t ADVERT_INTERVAL_MIN = 800;
        constexpr uint16_t ADVERT_INTERVAL_MAX = 1600;

        // Keep advertising while a client is connected so several phones /
        // a Home Assistant proxy can all see the device.
        class ServerCallbacks : public NimBLEServerCallbacks
        {
            void onConnect(NimBLEServer *server) override
            {
                if (server->getConnectedCount() < CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
                    NimBLEDevice::startAdvertising();
            }
        };

        NimBLECharacteristic *makeCharacteristic(NimBLEService *service, const char *uuid)
        {
            return service->createCharacteristic(uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        }
    }

    void BleService::begin(const std::string &deviceName)
    {
        if (active)
            return;

        const std::string name = deviceName.substr(0, MAX_ADVERTISED_NAME);
        NimBLEDevice::init(name);
        // WiFi (web UI, Alpaca, alerts) is the primary interface; BLE gets
        // what's left of the radio.
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);

        NimBLEServer *bleServer = NimBLEDevice::createServer();
        static ServerCallbacks callbacks;
        bleServer->setCallbacks(&callbacks, false);

        NimBLEService *service = bleServer->createService(Ble::SERVICE_UUID);
        safetyChar = makeCharacteristic(service, Ble::SAFETY_CHAR_UUID);
        rainChar = makeCharacteristic(service, Ble::RAIN_CHAR_UUID);
        alertChar = makeCharacteristic(service, Ble::ALERT_CHAR_UUID);
        summaryChar = makeCharacteristic(service, Ble::SUMMARY_CHAR_UUID);
        static_cast<NimBLECharacteristic *>(alertChar)->setValue("{}");
        static_cast<NimBLECharacteristic *>(summaryChar)->setValue("{}");
        service->start();
        server = bleServer;

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->addServiceUUID(Ble::SERVICE_UUID);
        advertising->setScanResponse(true);
        advertising->setMinInterval(ADVERT_INTERVAL_MIN);
        advertising->setMaxInterval(ADVERT_INTERVAL_MAX);
        advertising->start();

        active = true;
        Logger::info(TAG, "Advertising as \"%s\"", name.c_str());
    }

    uint8_t BleService::connectedClients() const
    {
        return active ? static_cast<uint8_t>(static_cast<NimBLEServer *>(server)->getConnectedCount()) : 0;
    }

    void BleService::update(const Ble::State &state, const std::string &summaryJson)
    {
        if (!active)
            return;

        const std::string safety = Ble::encodeSafety(state);
        if (safety != lastSafety)
        {
            auto *c = static_cast<NimBLECharacteristic *>(safetyChar);
            c->setValue(reinterpret_cast<const uint8_t *>(safety.data()), safety.size());
            c->notify();
            lastSafety = safety;
        }

        const std::string rain = Ble::encodeRain(state);
        if (rain != lastRain)
        {
            auto *c = static_cast<NimBLECharacteristic *>(rainChar);
            c->setValue(reinterpret_cast<const uint8_t *>(rain.data()), rain.size());
            c->notify();
            lastRain = rain;
        }

        const uint32_t now = millis();
        auto *summary = static_cast<NimBLECharacteristic *>(summaryChar);
        summary->setValue(summaryJson);
        if (now - lastSummaryAt >= SUMMARY_NOTIFY_INTERVAL_MS)
        {
            summary->notify();
            lastSummaryAt = now;
        }

        // Re-advertise when the safe/rain flags change (and refresh the SQM
        // value once a minute) - restarting advertising on every tick would
        // make the device flicker in scanners.
        const std::string advert = Ble::encodeAdvertisement(state);
        const bool flagsChanged = lastAdvert.size() < 6 || advert[5] != lastAdvert[5];
        if (flagsChanged || now - lastAdvertAt >= ADVERT_REFRESH_INTERVAL_MS)
        {
            NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
            advertising->setManufacturerData(advert);
            if (advertising->isAdvertising())
            {
                advertising->stop();
                advertising->start();
            }
            lastAdvert = advert;
            lastAdvertAt = now;
        }
    }

    void BleService::publishAlert(const Alerts::Alert &alert)
    {
        if (!active)
            return;
        StaticJsonDocument<384> doc;
        doc["event"] = Alerts::alertTypeName(alert.type);
        doc["title"] = alert.title;
        doc["message"] = alert.message;
        doc["priority"] = static_cast<int>(alert.priority);
        std::string json;
        serializeJson(doc, json);
        auto *c = static_cast<NimBLECharacteristic *>(alertChar);
        c->setValue(json);
        c->notify();
    }

} // namespace SQM

#else

namespace SQM
{
    void BleService::begin(const std::string &) {}
    uint8_t BleService::connectedClients() const { return 0; }
    void BleService::update(const Ble::State &, const std::string &) {}
    void BleService::publishAlert(const Alerts::Alert &) {}
} // namespace SQM

#endif
