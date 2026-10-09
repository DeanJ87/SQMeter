#include "WebServer.h"
#include "WebServerShared.h"
#include "Logger.h"
#include "version.h"
#include <WiFi.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <PubSubClient.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cstring>
#include <nvs.h>
#include <nvs_flash.h>
#include <ctime>
#include "calculations/CloudDetection.h"
#include "sensors/RG15Sensor.h"
#include "AlpacaDiscovery.h"
#include "Ipv6Network.h"
#include "DualStackClient.h"
#include "NetAddress.h"
#include "WiFiManager.h"
#include "HeapTrace.h"
#include "SunPosition.h"
#include "SafetyHistory.h"
#include <Preferences.h>

extern uint32_t bootCount;

// The safety verdict, alerts (engine, schedule, dispatch) and the /api/alerts and /api/safety routes.

namespace SQM
{
    using namespace WebShared;

    Alpaca::SafetyInputs WebServer::buildAlpacaSafetyInputs() const
    {
        return Core::safetyInputs(getSensorSnapshot(), getConfigCallback(), millis());
    }

    Alpaca::SafetyThresholds WebServer::buildAlpacaSafetyThresholds(const Config &cfg)
    {
        return Core::safetyThresholds(cfg);
    }

    Alpaca::SafetyResult WebServer::evaluateAlpacaSafety() const
    {
        return Alpaca::evaluateSafety(buildAlpacaSafetyInputs(), buildAlpacaSafetyThresholds(getConfigCallback()));
    }

    void WebServer::updateSafetyStatus()
    {
        const Alpaca::SafetyResult result = evaluateAlpacaSafety();
        const uint32_t now = millis();

        if (safetyMutex && xSemaphoreTake(safetyMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            const bool first = safetyStatus.evaluatedAtMs == 0;
            if (Core::updateSafety(safetyStatus, safeDelayFilter, result, getConfigCallback(), now))
            {
                if (!first)
                    Logger::info(TAG, "SafetyMonitor now %s", safetyStatus.isSafe ? "SAFE" : "UNSAFE");
                SafetyHistory::recordChange(safetyStatus.isSafe, !safetyStatus.isSafe && result.isSafe, result.reasonFlags);
            }
            xSemaphoreGive(safetyMutex);
        }

        processAlerts(getSafetyStatus());
    }

    Core::NightState WebServer::computeNight(const SensorSnapshot &snapshot, const Config &cfg)
    {
        return Core::night(snapshot, cfg, static_cast<int64_t>(time(nullptr)));
    }

    void WebServer::localClock(std::string &timeText, std::string &dateText)
    {
        char clock[8] = "--:--";
        char date[12] = "--";
        const time_t now = time(nullptr);
        if (now >= Core::CLOCK_VALID_EPOCH)
        {
            struct tm local;
            localtime_r(&now, &local);
            strftime(clock, sizeof(clock), "%H:%M", &local);
            strftime(date, sizeof(date), "%Y-%m-%d", &local);
        }
        timeText = clock;
        dateText = date;
    }

    void WebServer::processAlerts(const SafetyStatus &status)
    {
        const Config &cfg = getConfigCallback();
        if (mqttClient != nullptr && mqttClient->isConnected() && cfg.mqtt.publish.safety && status.evaluatedAtMs != 0)
            publishMqttSafety(status);

        const SensorSnapshot snapshot = getSensorSnapshot();
        const Alpaca::ObservingConditionsSnapshot obs = buildAlpacaObservingConditionsSnapshot();
        seedAlertEngine();
        const Core::NightState night = computeNight(snapshot, cfg);
        const Core::AlertSources sources{cfg, obs, night, status};
        Alerts::AlertInputs in = Core::alertInputs(sources, snapshot, millis());
        Core::LocalClock clock;
        localClock(clock.time, clock.date);
        watchClients(in, cfg, clock.time);
        if (ble.isActive())
            updateBle(in, sources);

        // Always run the engine so its state tracks reality while alerts are
        // off. Each alert gets its configured level, sound and wording;
        // everything raised in the same pass goes out as one notification.
        const time_t wallClock = time(nullptr);
        const uint32_t epoch = wallClock >= 1704067200 ? static_cast<uint32_t>(wallClock) : 0;
        const Core::AlertStep step = Core::runAlerts(alertEngine, in, Core::alertRules(cfg), sources, clock);
        sendAlerts(step, sources, snapshot, epoch);

        // Only after this pass's alerts went out: a clean disconnect's
        // "imaging app disconnected" is sent before it pauses alerts.
        const bool wasSending = alertSchedule.state().sending;
        if (alertSchedule.update(Core::effectiveSendMode(cfg), alpacaRouter.anyConnected(), millis(), epoch))
            scheduleChanged(wasSending);
        portENTER_CRITICAL(&scheduleLock);
        clientWatchShared = clientWatch;
        portEXIT_CRITICAL(&scheduleLock);

        forwardBleAcks(cfg, snapshot);
    }

    void WebServer::seedAlertEngine()
    {
        if (alertEngineSeeded)
            return;
        alertEngineSeeded = true;
        bool toldSafe = false;
        if (SafetyHistory::lastAlert(toldSafe))
            alertEngine.seedSafety(!toldSafe);
    }

    // The imaging app: is each Alpaca device still being checked?
    void WebServer::watchClients(Alerts::AlertInputs &in, const Config &cfg, const std::string &localTime)
    {
        const Alpaca::DeviceActivity activity[Alpaca::DEVICE_COUNT] = {
            alpacaRouter.activity(Alpaca::Device::SafetyMonitor), alpacaRouter.activity(Alpaca::Device::ObservingConditions)};
        uint32_t silenceMs[Alpaca::DEVICE_COUNT];
        Core::clientSilenceMs(cfg, silenceMs);
        clientWatch.update(activity, silenceMs, cfg.alpaca.enabled, millis());
        Core::addClientInputs(in, clientWatch, cfg, millis(), localTime);
    }

    void WebServer::updateBle(const Alerts::AlertInputs &in, const Core::AlertSources &sources)
    {
        const SafetyStatus &status = sources.status;
        const Alpaca::ObservingConditionsSnapshot &obs = sources.obs;
        Ble::State bleState;
        bleState.safetyKnown = in.safetyKnown;
        bleState.isSafe = status.isSafe;
        bleState.rawSafe = status.rawSafe;
        bleState.reasonFlags = status.reasonFlags;
        bleState.rainEnabled = sources.cfg.rain.enabled;
        bleState.rainHealthy = obs.rain.valid;
        bleState.raining = in.raining;
        bleState.rainRateMmPerHour = obs.rainRateMmPerHour;
        bleState.sqmValid = obs.skyLight.valid;
        bleState.sqm = obs.skyQualityMagArcsec2;

        StaticJsonDocument<256> summary;
        auto put = [&summary](const char *key, bool valid, float value)
        {
            if (valid)
                summary[key] = serialized(String(value, 2));
        };
        put("sqm", obs.skyLight.valid, obs.skyQualityMagArcsec2);
        put("cloud", obs.irSky.valid, obs.cloudCoverPercent);
        put("skyT", obs.irSky.valid, obs.skyTemperatureC);
        put("temp", obs.environment.valid, obs.temperatureC);
        put("hum", obs.environment.valid, obs.humidityPercent);
        put("dew", obs.environment.valid, obs.dewpointC);
        put("press", obs.environment.valid, obs.pressureHPa);
        put("wind", obs.wind.valid, obs.windSpeedMs);
        put("gust", obs.wind.valid, obs.windGustMs);
        std::string summaryJson;
        serializeJson(summary, summaryJson);
        ble.update(bleState, summaryJson);
    }

    // Push channels need the master switch, paired phones only Bluetooth.
    // Paused: state is still tracked, nothing goes out.
    void WebServer::sendAlerts(
        const Core::AlertStep &step, const Core::AlertSources &sources, const SensorSnapshot &snapshot, uint32_t epoch)
    {
        if (step.outgoing.empty() || !alertSchedule.state().sending)
            return;
        const Config &cfg = sources.cfg;
        const Alerts::Alert notification = Alerts::stackAlerts(step.outgoing);
        if (cfg.alerts.enabled)
        {
            alertDispatcher->dispatch(notification, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(snapshot));
            ble.publishAlert(notification);
            for (const Alerts::Alert &sent : step.outgoing)
                if (sent.type == Alerts::AlertType::Unsafe || sent.type == Alerts::AlertType::Safe)
                    SafetyHistory::recordAlert(sent.type == Alerts::AlertType::Safe);
        }
        if (notification.level == Alerts::AlertLevel::Wake)
            ble.raiseAlarm(step.alarmFlags, epoch);
        else
            ble.raiseInfo(sources.status.reasonFlags, epoch);
    }

    void WebServer::forwardBleAcks(const Config &cfg, const SensorSnapshot &snapshot)
    {
        uint32_t acknowledged = 0;
        bool fromPhone = false;
        if (!ble.processAcks(acknowledged, fromPhone) || !cfg.alerts.enabled)
            return;
        Alerts::Alert ack;
        ack.type = Alerts::AlertType::Acknowledged;
        ack.level = Alerts::AlertLevel::Quiet;
        ack.title = "Alarm acknowledged";
        ack.message = std::string("Phone alarm #") + std::to_string(acknowledged) + " was acknowledged " +
                      (fromPhone ? "on a phone." : "in the web UI.");
        alertDispatcher->dispatch(ack, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(snapshot));
    }

    void WebServer::applyPendingArm()
    {
        const int8_t requested = pendingArm.exchange(-1);
        if (requested < 0)
            return;
        const bool wasSending = alertSchedule.state().sending;
        const time_t wallClock = time(nullptr);
        const int64_t epoch = wallClock >= Core::CLOCK_VALID_EPOCH ? static_cast<int64_t>(wallClock) : 0;
        if (alertSchedule.command((requested & 1) != 0, static_cast<Alerts::ScheduleReason>(requested / 2), millis(), epoch))
            scheduleChanged(wasSending);
    }

    void WebServer::shareSchedule()
    {
        portENTER_CRITICAL(&scheduleLock);
        scheduleShared = alertSchedule.state();
        portEXIT_CRITICAL(&scheduleLock);
    }

    Alerts::ScheduleState WebServer::sharedSchedule() const
    {
        portENTER_CRITICAL(&scheduleLock);
        Alerts::ScheduleState state = scheduleShared;
        portEXIT_CRITICAL(&scheduleLock);
        // A pause/resume the loop hasn't applied yet already counts.
        const int8_t pending = pendingArm.load();
        if (pending >= 0)
        {
            state.sending = (pending & 1) != 0;
            state.reason = static_cast<Alerts::ScheduleReason>(pending / 2);
        }
        return state;
    }

    Alpaca::ClientWatch WebServer::sharedClientWatch() const
    {
        portENTER_CRITICAL(&scheduleLock);
        const Alpaca::ClientWatch copy = clientWatchShared;
        portEXIT_CRITICAL(&scheduleLock);
        return copy;
    }

    void WebServer::scheduleChanged(bool wasSending)
    {
        const Alerts::ScheduleState &state = alertSchedule.state();
        shareSchedule();
        Preferences prefs;
        if (prefs.begin(ARMED_NVS_NAMESPACE, false))
        {
            prefs.putBool("armed", state.sending);
            prefs.putUChar("reason", static_cast<uint8_t>(state.reason));
            prefs.putUInt("since", static_cast<uint32_t>(state.sinceEpoch > 0 ? state.sinceEpoch : 0));
            prefs.end();
        }
        if (state.sending == wasSending)
            return;
        Logger::info(TAG, "Alerts %s (%s)", state.sending ? "resumed" : "paused", Alerts::scheduleReasonName(state.reason));
        SafetyHistory::recordArmed(state.sending);
        publishArmedState();

        const Config &cfg = getConfigCallback();
        if (state.sending && cfg.alerts.enabled)
        {
            // One quiet line so you know where things stand as you start.
            const SafetyStatus safety = getSafetyStatus();
            Alerts::Alert on;
            on.type = Alerts::AlertType::AlertsOn;
            on.level = Alerts::AlertLevel::Quiet;
            on.title = "Alerts resumed";
            if (safety.evaluatedAtMs == 0)
                on.message = "Safety not evaluated yet.";
            else if (safety.isSafe)
                on.message = "Observatory safe.";
            else
                on.message =
                    "Observatory UNSAFE" + (safety.reasons.empty() ? std::string(".") : ":\n" + Alerts::joinReasons(safety.reasons));
            alertDispatcher->dispatch(on, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(getSensorSnapshot()));
        }
    }

    void WebServer::publishArmedState()
    {
        if (mqttClient == nullptr || !mqttClient->isConnected())
            return;
        if (mqttClient->publishSubtopic("alerts/armed", alertSchedule.state().sending ? "1" : "0", true))
            mqttArmedConnection = mqttClient->connectionCount();
    }

    void WebServer::publishMqttSafety(const SafetyStatus &status)
    {
        const uint32_t now = millis();
        const bool changed =
            !mqttSafetyPublished || status.isSafe != mqttLastPublishedSafe || mqttClient->connectionCount() != mqttSafetyConnection;
        if (!changed && now - mqttSafetyPublishedAt < MQTT_SAFETY_REPUBLISH_MS)
            return;

        // Same object as GET /api/safety; <base>/safe is the bare 1/0 for
        // loggers, simple automations and the Home Assistant binary sensor.
        DynamicJsonDocument doc(1024);
        appendSafetyStatus(doc.to<JsonObject>());
        std::string payload;
        serializeJson(doc, payload);
        if (mqttClient->publishSubtopic("safety", payload, true) && mqttClient->publishSubtopic("safe", status.isSafe ? "1" : "0", true))
        {
            mqttSafetyPublished = true;
            mqttLastPublishedSafe = status.isSafe;
            mqttSafetyPublishedAt = now;
            mqttSafetyConnection = mqttClient->connectionCount();
        }
    }

    namespace
    {
        // ?channel= of a test send: the channel bits, false if unknown.
        bool testChannelMask(const String &channel, uint8_t &mask)
        {
            if (channel == "all")
                mask = ALERT_CHANNELS_ALL;
            else if (channel == "mqtt")
                mask = alertChannelBit(AlertChannel::Mqtt);
            else if (channel == "pushover")
                mask = alertChannelBit(AlertChannel::Pushover);
            else if (channel == "ntfy")
                mask = alertChannelBit(AlertChannel::Ntfy);
            else if (channel == "webhook")
                mask = alertChannelBit(AlertChannel::Webhook);
            else
                return false;
            return true;
        }

        uint8_t enabledChannelMask(const AlertsConfig &alerts)
        {
            return (alerts.mqttEnabled ? alertChannelBit(AlertChannel::Mqtt) : 0) |
                   (alerts.pushoverEnabled ? alertChannelBit(AlertChannel::Pushover) : 0) |
                   (alerts.ntfyEnabled ? alertChannelBit(AlertChannel::Ntfy) : 0) |
                   (alerts.webhookEnabled ? alertChannelBit(AlertChannel::Webhook) : 0);
        }

        String paramOr(AsyncWebServerRequest *request, const char *name, const String &fallback)
        {
            return request->hasParam(name) ? request->getParam(name)->value() : fallback;
        }

        // ?event=rain_started&level=4&sound=siren&title=..&message=..: a
        // sample of that event at the given level and wording, so unsaved
        // choices can be tried out.
        struct TestSample
        {
            const Core::SampleAlert *sample = nullptr; // nullptr: the generic test
            uint8_t level = 2;
            String sound;
            String title;
            String message;
        };

        // nullptr when the request is valid, else the error to send.
        const char *readTestSample(AsyncWebServerRequest *request, TestSample &out)
        {
            if (!request->hasParam("event"))
                return nullptr;
            out.sample = Core::sampleAlert(request->getParam("event")->value().c_str());
            if (out.sample == nullptr)
                return "Unknown event";
            out.level = request->hasParam("level") ? static_cast<uint8_t>(request->getParam("level")->value().toInt()) : 2;
            if (out.level < 1 || out.level > 4)
                return "Level must be 1-4";
            out.sound = paramOr(request, "sound", String());
            if (out.sound.length() > 32)
                return "Sound name too long";
            out.title = paramOr(request, "title", String());
            out.message = paramOr(request, "message", String());
            if (!AlertsConfig::templateFits(out.title.c_str(), out.message.c_str()))
                return "Title is up to 80 characters and message up to 240";
            return nullptr;
        }
    } // namespace

    void WebServer::setupAlertRoutes()
    {
        server.on("/api/alerts/test", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAlertTest(request); });
        server.on("/api/alerts/clear", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAlertsClear(request); });
        server.on("/api/ble/ack", HTTP_POST, [this](AsyncWebServerRequest *request) { handleBleAck(request); });
        server.on("/api/ble/forget-bonds", HTTP_POST, [this](AsyncWebServerRequest *request) { handleBleForgetBonds(request); });

        // Resume ("arm") and pause ("disarm") for automations (Home Assistant
        // rest_command, N.I.N.A. sequence scripts) and the web UI, which adds
        // ?source=ui so the status line can say who paused.
        server.on("/api/alerts/arm", HTTP_POST, [this](AsyncWebServerRequest *request) { handleArm(request, true); });
        server.on("/api/alerts/disarm", HTTP_POST, [this](AsyncWebServerRequest *request) { handleArm(request, false); });
        server.on("/api/alerts/armed", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAlertsArmed(request); });
        server.on("/api/alerts/recent", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAlertsRecent(request); });
    }

    void WebServer::handleAlertTest(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        uint8_t mask = 0;
        if (!testChannelMask(paramOr(request, "channel", String("all")), mask))
        {
            request->send(
                400, "application/json", createErrorJson("Unknown channel (expected mqtt, pushover, ntfy, webhook or all)").c_str());
            return;
        }
        TestSample test;
        if (const char *error = readTestSample(request, test))
        {
            request->send(400, "application/json", createErrorJson(error).c_str());
            return;
        }

        uint8_t sendMask = mask & enabledChannelMask(getConfigCallback().alerts);
        const bool ringsPhone = test.sample != nullptr && test.level == 4 && ble.alarmStatus().serviceActive;
        if (sendMask == 0 && ringsPhone)
            sendMask = BLE_ONLY_TEST;
        if (sendMask == 0)
        {
            request->send(
                400, "application/json", createErrorJson("That channel isn't enabled - enable it and save settings first").c_str());
            return;
        }

        portENTER_CRITICAL(&pendingAlertTestLock);
        pendingAlertTest.mask |= sendMask;
        pendingAlertTest.sample = test.sample;
        pendingAlertTest.level = test.level;
        strlcpy(pendingAlertTest.sound, test.sound.c_str(), sizeof(pendingAlertTest.sound));
        strlcpy(pendingAlertTest.title, test.title.c_str(), sizeof(pendingAlertTest.title));
        strlcpy(pendingAlertTest.message, test.message.c_str(), sizeof(pendingAlertTest.message));
        portEXIT_CRITICAL(&pendingAlertTestLock);
        request->send(202, "application/json", "{\"success\":true,\"message\":\"Test notification queued\"}");
    }

    void WebServer::handleAlertsClear(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        alertDispatcher->clearRecent();
        request->send(200, "application/json", "{\"success\":true}");
    }

    void WebServer::handleBleAck(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        if (!ble.alarmStatus().alarmActive)
        {
            request->send(409, "application/json", createErrorJson("No phone alarm is active").c_str());
            return;
        }
        ble.requestLocalAck();
        request->send(202, "application/json", "{\"success\":true}");
    }

    void WebServer::handleBleForgetBonds(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        if (!ble.isActive())
        {
            request->send(409, "application/json", createErrorJson("Bluetooth is off").c_str());
            return;
        }
        ble.requestForgetBonds();
        request->send(202, "application/json", "{\"success\":true}");
    }

    void WebServer::handleArm(AsyncWebServerRequest *request, bool armed)
    {
        if (!requireAuth(request))
            return;
        const AsyncWebParameter *source = request->getParam("source");
        const bool fromUi = source != nullptr && source->value() == "ui";
        pendingArm = encodeArm(armed, fromUi ? Alerts::ScheduleReason::UserUi : Alerts::ScheduleReason::UserRest);
        request->send(202, "application/json", armed ? "{\"success\":true,\"armed\":true}" : "{\"success\":true,\"armed\":false}");
    }

    void WebServer::handleAlertsArmed(AsyncWebServerRequest *request)
    {
        StaticJsonDocument<384> doc;
        Core::writeAlertSchedule(doc.to<JsonObject>(), sharedSchedule(), getConfigCallback(), millis());
        std::string json;
        serializeJson(doc, json);
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleAlertsRecent(AsyncWebServerRequest *request)
    {
        const std::vector<AlertRecord> records = alertDispatcher->recent();
        DynamicJsonDocument doc(8192);
        doc["enabled"] = getConfigCallback().alerts.enabled;
        doc["armed"] = sharedSchedule().sending;
        JsonArray arr = doc.createNestedArray("alerts");
        const uint32_t nowSeconds = millis() / 1000;
        for (auto it = records.rbegin(); it != records.rend(); ++it) // newest first
        {
            JsonObject item = arr.createNestedObject();
            item["id"] = it->id;
            item["event"] = Alerts::alertTypeName(it->alert.type);
            item["title"] = it->alert.title;
            item["message"] = it->alert.message;
            item["level"] = Alerts::alertLevelName(it->alert.level);
            item["ageSeconds"] = nowSeconds - it->uptimeSeconds;
            if (it->epochSeconds != 0)
                item["timestamp"] = it->epochSeconds;
            JsonObject channels = item.createNestedObject("channels");
            for (size_t i = 0; i < ALERT_CHANNEL_COUNT; ++i)
            {
                if (it->status[i] == DeliveryStatus::NotSent)
                    continue;
                JsonObject ch = channels.createNestedObject(alertChannelName(static_cast<AlertChannel>(i)));
                ch["status"] = deliveryStatusName(it->status[i]);
                ch["detail"] = it->detail[i];
            }
        }
        std::string json;
        serializeJson(doc, json);
        request->send(200, "application/json", json.c_str());
    }

    SafetyStatus WebServer::getSafetyStatus() const
    {
        SafetyStatus copy;
        if (safetyMutex && xSemaphoreTake(safetyMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            copy = safetyStatus;
            xSemaphoreGive(safetyMutex);
        }
        return copy;
    }

} // namespace SQM
