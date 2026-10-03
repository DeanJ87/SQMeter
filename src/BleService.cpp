#include "BleService.h"

#if SQM_ENABLE_BLE

#include "Logger.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <esp_coexist.h>
#include <esp_bt.h>

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
        constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;

        // Alarm, ack and heartbeat only work over a bonded link that was
        // paired with the passkey (MITM-protected). NimBLE also skips
        // notifications/indications to links that aren't encrypted when the
        // characteristic carries the *_ENC flags, so an unpaired phone never
        // hears an alarm.
        constexpr uint32_t SECURE_READ = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN;
        constexpr uint32_t SECURE_WRITE = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;

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

        class AckCallbacks : public NimBLECharacteristicCallbacks
        {
        public:
            explicit AckCallbacks(BleService &owner) : owner(owner) {}

            void onWrite(NimBLECharacteristic *characteristic) override
            {
                const std::string value = characteristic->getValue();
                uint32_t seq = 0;
                if (Ble::decodeAck(reinterpret_cast<const uint8_t *>(value.data()), value.size(), seq) && seq != 0)
                    owner.onPhoneAck(seq);
            }

        private:
            BleService &owner;
        };
    }

    void BleService::begin(const std::string &deviceName, const std::string &passkey)
    {
        if (active)
            return;

        const std::string name = deviceName.substr(0, MAX_ADVERTISED_NAME);
        NimBLEDevice::init(name);
        // WiFi (web UI, Alpaca, alerts) is the primary interface; BLE gets
        // what's left of the radio.
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);

        uint32_t pin = 0;
        alarmService = Ble::parsePasskey(passkey, pin);
        if (alarmService)
        {
            // Static passkey "displayed" by the device (the user reads it from
            // Settings) and typed on the phone: bonded, MITM-protected,
            // LE Secure Connections.
            NimBLEDevice::setSecurityAuth(true, true, true);
            NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
            NimBLEDevice::setSecurityPasskey(pin);
        }

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

        if (alarmService)
        {
            auto *alarmCharacteristic = service->createCharacteristic(Ble::ALARM_CHAR_UUID, SECURE_READ | NIMBLE_PROPERTY::INDICATE);
            alarmCharacteristic->setValue(alarm.encode());
            alarmChar = alarmCharacteristic;

            auto *ackCharacteristic = service->createCharacteristic(Ble::ACK_CHAR_UUID, SECURE_WRITE);
            static AckCallbacks ackCallbacks(*this);
            ackCharacteristic->setCallbacks(&ackCallbacks);

            auto *heartbeatCharacteristic = service->createCharacteristic(Ble::HEARTBEAT_CHAR_UUID, SECURE_READ | NIMBLE_PROPERTY::NOTIFY);
            heartbeatCharacteristic->setValue(Ble::encodeHeartbeat(0, millis() / 1000));
            heartbeatChar = heartbeatCharacteristic;
        }
        service->start();
        server = bleServer;

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->addServiceUUID(Ble::SERVICE_UUID);
        advertising->setScanResponse(true);
        advertising->setMinInterval(ADVERT_INTERVAL_MIN);
        advertising->setMaxInterval(ADVERT_INTERVAL_MAX);
        advertising->start();

        active = true;
        Logger::info(TAG, "Advertising as \"%s\"%s (%d bonded phone%s)", name.c_str(),
                     alarmService ? " with the phone alarm service" : "", NimBLEDevice::getNumBonds(),
                     NimBLEDevice::getNumBonds() == 1 ? "" : "s");
    }

    void BleService::releaseControllerMemory()
    {
        if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
            esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    }

    BleAlarmStatus BleService::alarmStatus() const
    {
        BleAlarmStatus status;
        status.serviceActive = active && alarmService;
        status.alarmActive = alarm.active();
        status.sequence = alarm.sequence();
        status.acknowledgedSequence = alarm.acknowledgedSeq();
        status.bondedPhones = active ? NimBLEDevice::getNumBonds() : 0;
        return status;
    }

    void BleService::sendAlarm(uint32_t nowMs)
    {
        auto *c = static_cast<NimBLECharacteristic *>(alarmChar);
        const std::string payload = alarm.encode();
        c->setValue(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
        c->indicate();
        alarm.markSent(nowMs);
    }

    void BleService::raiseAlarm(uint32_t reasonFlags, uint32_t epochSeconds)
    {
        if (!active || !alarmService)
            return;
        const uint32_t now = millis();
        if (alarm.raiseAlarm(reasonFlags, epochSeconds, now))
        {
            Logger::warn(TAG, "Phone alarm #%u raised", static_cast<unsigned>(alarm.sequence()));
            sendAlarm(now);
        }
    }

    void BleService::raiseInfo(uint32_t reasonFlags, uint32_t epochSeconds)
    {
        if (!active || !alarmService)
            return;
        const uint32_t now = millis();
        if (alarm.raiseInfo(reasonFlags, epochSeconds, now))
            sendAlarm(now);
    }

    bool BleService::processAcks(uint32_t &acknowledgedSeq, bool &fromPhone)
    {
        if (!active)
            return false;

        if (forgetBondsRequested.exchange(false))
        {
            NimBLEDevice::deleteAllBonds();
            Logger::warn(TAG, "Forgot all bonded phones");
        }

        if (!alarmService)
            return false;

        const uint32_t phoneSeq = pendingPhoneAck.exchange(0);
        const bool local = localAckRequested.exchange(false);
        const uint32_t now = millis();
        uint32_t seq = 0;
        if (phoneSeq != 0 && alarm.acknowledge(phoneSeq, now))
        {
            seq = phoneSeq;
            fromPhone = true;
        }
        else if (local && alarm.acknowledge(alarm.sequence(), now))
        {
            seq = alarm.sequence();
            fromPhone = false;
        }
        if (seq == 0)
            return false;

        // Tell every phone, so the others stop ringing too.
        sendAlarm(now);
        acknowledgedSeq = seq;
        Logger::info(TAG, "Phone alarm #%u acknowledged %s", static_cast<unsigned>(seq), fromPhone ? "on a phone" : "in the web UI");
        return true;
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

        if (alarmService)
        {
            if (alarm.resendDue(now))
                sendAlarm(now);
            if (lastHeartbeatAt == 0 || now - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS)
            {
                auto *heartbeat = static_cast<NimBLECharacteristic *>(heartbeatChar);
                const std::string payload = Ble::encodeHeartbeat(++heartbeatSeq, now / 1000);
                heartbeat->setValue(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
                heartbeat->notify();
                lastHeartbeatAt = now;
            }
        }

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
    void BleService::releaseControllerMemory() {}
    void BleService::begin(const std::string &, const std::string &) {}
    uint8_t BleService::connectedClients() const { return 0; }
    BleAlarmStatus BleService::alarmStatus() const { return {}; }
    void BleService::update(const Ble::State &, const std::string &) {}
    void BleService::publishAlert(const Alerts::Alert &) {}
    void BleService::raiseAlarm(uint32_t, uint32_t) {}
    void BleService::raiseInfo(uint32_t, uint32_t) {}
    bool BleService::processAcks(uint32_t &, bool &) { return false; }
} // namespace SQM

#endif
