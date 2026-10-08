#include "AlertDispatcher.h"
#include "AlertRootCA.h"
#include "Logger.h"
#include "MQTTClient.h"
#include "TlsLock.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctime>
#include <memory>

namespace SQM
{
    // Several events sent as one notification: list them all, lead first.
    static void appendStackedEvents(JsonDocument &doc, const Alerts::Alert &alert)
    {
        if (alert.stacked.empty())
            return;
        JsonArray events = doc.createNestedArray("events");
        events.add(Alerts::alertTypeName(alert.type));
        for (Alerts::AlertType type : alert.stacked)
            events.add(Alerts::alertTypeName(type));
    }

    namespace
    {
        constexpr const char *TAG = "Alerts";
        constexpr int HTTP_TIMEOUT_MS = 10000;
        constexpr size_t MAX_ERROR_BODY_CHARS = 120;

        std::string urlEncode(const std::string &value)
        {
            static const char *hex = "0123456789ABCDEF";
            std::string out;
            out.reserve(value.size() * 3);
            for (unsigned char c : value)
            {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~')
                {
                    out += static_cast<char>(c);
                }
                else
                {
                    out += '%';
                    out += hex[c >> 4];
                    out += hex[c & 0x0F];
                }
            }
            return out;
        }

        std::string fullTitle(const std::string &deviceName, const std::string &title)
        {
            return deviceName.empty() ? title : deviceName + ": " + title;
        }

        int64_t epochNow()
        {
            const time_t now = time(nullptr);
            return now >= 1704067200 ? static_cast<int64_t>(now) : 0;
        }

        bool httpPost(const std::string &url, const char *contentType, const std::string &body,
                      const std::vector<std::pair<std::string, std::string>> &headers, bool insecureTls,
                      std::string &detail, const char *rootCa = ALERT_ROOT_CA_PEM)
        {
            std::unique_ptr<WiFiClient> client;
            if (url.rfind("https://", 0) == 0)
            {
                auto secure = std::make_unique<WiFiClientSecure>();
                if (insecureTls)
                    secure->setInsecure();
                else
                    secure->setCACert(rootCa);
                client = std::move(secure);
            }
            else
            {
                client = std::make_unique<WiFiClient>();
            }

            HTTPClient http;
            http.setTimeout(HTTP_TIMEOUT_MS);
            http.setConnectTimeout(HTTP_TIMEOUT_MS);
            if (!http.begin(*client, url.c_str()))
            {
                detail = "Invalid URL";
                return false;
            }
            http.addHeader("Content-Type", contentType);
            for (const auto &header : headers)
                http.addHeader(header.first.c_str(), header.second.c_str());

            const int code = http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.data())), body.size());
            if (code <= 0)
            {
                detail = HTTPClient::errorToString(code).c_str();
                http.end();
                return false;
            }

            detail = "HTTP " + std::to_string(code);
            const bool ok = code >= 200 && code < 300;
            if (!ok)
            {
                String response = http.getString();
                // Pushover (and ntfy) explain failures in JSON: show the
                // message itself rather than truncated raw JSON.
                StaticJsonDocument<512> errorDoc;
                if (!deserializeJson(errorDoc, response))
                {
                    const char *message = errorDoc["errors"][0] | errorDoc["error"] | static_cast<const char *>(nullptr);
                    if (message != nullptr)
                        response = message;
                }
                if (response.length() > 0)
                    detail += ": " + std::string(response.substring(0, MAX_ERROR_BODY_CHARS).c_str());
            }
            http.end();
            return ok;
        }

        const char *ntfyTags(Alerts::AlertType type)
        {
            switch (type)
            {
            case Alerts::AlertType::Unsafe:
                return "warning";
            case Alerts::AlertType::Safe:
                return "white_check_mark";
            case Alerts::AlertType::RainStarted:
                return "cloud_with_rain";
            case Alerts::AlertType::RainStopped:
                return "sun_behind_small_cloud";
            case Alerts::AlertType::SensorFault:
            case Alerts::AlertType::LensFault:
                return "rotating_light";
            case Alerts::AlertType::SensorRecovered:
                return "wrench";
            case Alerts::AlertType::DewRisk:
                return "droplet";
            case Alerts::AlertType::ClearSky:
                return "star";
            case Alerts::AlertType::CloudedOver:
                return "cloud";
            case Alerts::AlertType::Acknowledged:
                return "ok_hand";
            case Alerts::AlertType::AlertsOn:
                return "telescope";
            case Alerts::AlertType::Test:
                return "test_tube";
            }
            return "bell";
        }
    }

    const char *alertChannelName(AlertChannel channel)
    {
        switch (channel)
        {
        case AlertChannel::Mqtt:
            return "mqtt";
        case AlertChannel::Pushover:
            return "pushover";
        case AlertChannel::Ntfy:
            return "ntfy";
        case AlertChannel::Webhook:
            return "webhook";
        }
        return "unknown";
    }

    const char *deliveryStatusName(DeliveryStatus status)
    {
        switch (status)
        {
        case DeliveryStatus::NotSent:
            return "not_sent";
        case DeliveryStatus::Pending:
            return "pending";
        case DeliveryStatus::Sent:
            return "sent";
        case DeliveryStatus::Failed:
            return "failed";
        case DeliveryStatus::Skipped:
            return "skipped";
        }
        return "unknown";
    }

    AlertDispatcher::AlertDispatcher(MQTTClient *mqttClient, BusyCheck busy)
        : mqtt(mqttClient), networkBusy(std::move(busy))
    {
    }

    AlertDispatcher::~AlertDispatcher()
    {
        // The dispatcher lives for the life of the firmware; the task is
        // never stopped, so the queue/mutex are intentionally not deleted.
    }

    void AlertDispatcher::begin()
    {
        mutex = xSemaphoreCreateMutex();
        queue = xQueueCreate(QUEUE_LENGTH, sizeof(Job *));
    }

    void AlertDispatcher::ensureTask()
    {
        bool expected = false;
        if (!taskRunning.compare_exchange_strong(expected, true))
            return;
        if (xTaskCreatePinnedToCore(taskEntry, "alerts", TASK_STACK_WORDS, this, 1, nullptr, 1) != pdPASS)
        {
            taskRunning.store(false);
            Logger::error(TAG, "Couldn't start the alert task (low memory)");
        }
    }

    void AlertDispatcher::taskEntry(void *arg)
    {
        static_cast<AlertDispatcher *>(arg)->run();
    }

    void AlertDispatcher::dispatch(const Alerts::Alert &alert, const AlertsConfig &cfg, const std::string &deviceName, uint8_t channelMask)
    {
        AlertRecord record;
        record.uptimeSeconds = millis() / 1000;
        record.epochSeconds = epochNow();
        record.alert = alert;

        const bool wanted[ALERT_CHANNEL_COUNT] = {
            cfg.mqttEnabled && (channelMask & alertChannelBit(AlertChannel::Mqtt)),
            cfg.pushoverEnabled && (channelMask & alertChannelBit(AlertChannel::Pushover)),
            cfg.ntfyEnabled && (channelMask & alertChannelBit(AlertChannel::Ntfy)),
            cfg.webhookEnabled && (channelMask & alertChannelBit(AlertChannel::Webhook)),
        };

        // MQTT: publish right here on the main loop task.
        if (wanted[0])
        {
            DynamicJsonDocument doc(768);
            doc["event"] = Alerts::alertTypeName(alert.type);
            appendStackedEvents(doc, alert);
            doc["title"] = alert.title;
            doc["message"] = alert.message;
            doc["level"] = Alerts::alertLevelName(alert.level);
            doc["device"] = deviceName;
            if (record.epochSeconds != 0)
                doc["timestamp"] = record.epochSeconds;
            std::string payload;
            serializeJson(doc, payload);
            const bool ok = mqtt != nullptr && mqtt->publishSubtopic("alerts", payload, false);
            record.status[0] = ok ? DeliveryStatus::Sent : DeliveryStatus::Failed;
            record.detail[0] = ok ? "Published" : "MQTT not connected";
        }

        uint8_t httpMask = 0;
        for (size_t i = 1; i < ALERT_CHANNEL_COUNT; ++i)
        {
            if (wanted[i])
            {
                record.status[i] = DeliveryStatus::Pending;
                httpMask |= static_cast<uint8_t>(1u << i);
            }
        }

        uint32_t id = 0;
        if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            id = nextId++;
            record.id = id;
            records.push_back(record);
            if (records.size() > MAX_RECORDS)
                records.erase(records.begin());
            xSemaphoreGive(mutex);
        }

        Logger::info(TAG, "%s: %s", alert.title.c_str(), alert.message.c_str());

        if (httpMask == 0 || queue == nullptr || id == 0)
            return;

        Job *job = new Job{id, alert, cfg, deviceName, httpMask};
        if (xQueueSend(queue, &job, 0) == pdTRUE)
        {
            ensureTask();
        }
        else
        {
            Logger::warn(TAG, "Alert queue full - dropping notification");
            for (size_t i = 1; i < ALERT_CHANNEL_COUNT; ++i)
                if (httpMask & (1u << i))
                    setStatus(id, static_cast<AlertChannel>(i), DeliveryStatus::Failed, "Queue full");
            delete job;
        }
    }

    void AlertDispatcher::run()
    {
        for (;;)
        {
            Job *job = nullptr;
            if (xQueueReceive(queue, &job, pdMS_TO_TICKS(TASK_IDLE_EXIT_MS)) == pdTRUE)
            {
                if (job != nullptr)
                {
                    deliver(*job);
                    delete job;
                }
                continue;
            }

            // Idle: exit and give the stack back. If a job raced in after the
            // timeout, take ownership again instead of exiting.
            taskRunning.store(false);
            bool expected = false;
            if (uxQueueMessagesWaiting(queue) > 0 && taskRunning.compare_exchange_strong(expected, true))
                continue;
            vTaskDelete(nullptr);
        }
    }

    void AlertDispatcher::deliver(const Job &job)
    {
        using Sender = bool (AlertDispatcher::*)(const Job &, std::string &);
        const struct
        {
            AlertChannel channel;
            Sender send;
        } senders[] = {
            {AlertChannel::Pushover, &AlertDispatcher::sendPushover},
            {AlertChannel::Ntfy, &AlertDispatcher::sendNtfy},
            {AlertChannel::Webhook, &AlertDispatcher::sendWebhook},
        };

        for (const auto &sender : senders)
        {
            if (!(job.channelMask & alertChannelBit(sender.channel)))
                continue;

            if (WiFi.status() != WL_CONNECTED)
            {
                setStatus(job.recordId, sender.channel, DeliveryStatus::Skipped, "WiFi not connected");
                continue;
            }
            if (networkBusy && networkBusy())
            {
                // TLS needs ~40 KB of heap; an OTA download already holds one session.
                setStatus(job.recordId, sender.channel, DeliveryStatus::Skipped, "Firmware update in progress");
                continue;
            }

            TlsLock::Guard tls(30000);
            if (!tls.ok())
            {
                setStatus(job.recordId, sender.channel, DeliveryStatus::Skipped, "Another HTTPS request is in progress");
                continue;
            }

            std::string detail;
            bool ok = (this->*sender.send)(job, detail);
            if (!ok && detail.rfind("HTTP ", 0) != 0)
            {
                // Connection-level failure (DNS, TCP, TLS memory): one retry.
                vTaskDelay(pdMS_TO_TICKS(3000));
                ok = (this->*sender.send)(job, detail);
            }
            setStatus(job.recordId, sender.channel, ok ? DeliveryStatus::Sent : DeliveryStatus::Failed, detail);
            if (!ok)
                Logger::warn(TAG, "%s delivery failed: %s", alertChannelName(sender.channel), detail.c_str());
        }
    }

    bool AlertDispatcher::sendPushover(const Job &job, std::string &detail)
    {
        // quiet -1 (no sound), normal 0, urgent 1 (bypasses quiet hours),
        // wake 2 (emergency: repeats until acknowledged).
        const int priority = static_cast<int>(job.alert.level) - 2;

        std::string body = "token=" + urlEncode(job.cfg.pushoverAppToken) +
                           "&user=" + urlEncode(job.cfg.pushoverUserKey) +
                           "&title=" + urlEncode(fullTitle(job.deviceName, job.alert.title)) +
                           "&message=" + urlEncode(job.alert.message) +
                           "&priority=" + std::to_string(priority);
        if (priority == 2)
            body += "&retry=60&expire=3600"; // emergency: repeat every minute for up to an hour until acknowledged
        const std::string &sound = job.alert.sound.empty() ? job.cfg.pushoverSound : job.alert.sound;
        if (!sound.empty())
            body += "&sound=" + urlEncode(sound);

        return httpPost("https://api.pushover.net/1/messages.json", "application/x-www-form-urlencoded", body, {}, false, detail,
                        PUSHOVER_ROOT_CA_PEM);
    }

    bool AlertDispatcher::sendNtfy(const Job &job, std::string &detail)
    {
        std::string server = job.cfg.ntfyServer;
        while (!server.empty() && server.back() == '/')
            server.pop_back();

        static const char *const NTFY_PRIORITY[] = {"min", "low", "default", "high", "max"};
        const char *priority = NTFY_PRIORITY[static_cast<uint8_t>(job.alert.level) <= 4 ? static_cast<uint8_t>(job.alert.level) : 2];
        std::vector<std::pair<std::string, std::string>> headers = {
            {"Title", fullTitle(job.deviceName, job.alert.title)},
            {"Priority", priority},
            {"Tags", ntfyTags(job.alert.type)},
        };
        if (!job.cfg.ntfyToken.empty())
            headers.push_back({"Authorization", "Bearer " + job.cfg.ntfyToken});

        const bool ntfySh = server == "https://ntfy.sh";
        return httpPost(server + "/" + urlEncode(job.cfg.ntfyTopic), "text/plain; charset=utf-8", job.alert.message, headers, false, detail,
                        ntfySh ? NTFY_SH_ROOT_CA_PEM : ALERT_ROOT_CA_PEM);
    }

    bool AlertDispatcher::sendWebhook(const Job &job, std::string &detail)
    {
        DynamicJsonDocument doc(768);
        doc["device"] = job.deviceName;
        doc["event"] = Alerts::alertTypeName(job.alert.type);
        appendStackedEvents(doc, job.alert);
        doc["title"] = job.alert.title;
        doc["message"] = job.alert.message;
        doc["level"] = Alerts::alertLevelName(job.alert.level);
        const int64_t epoch = epochNow();
        if (epoch != 0)
            doc["timestamp"] = epoch;
        std::string body;
        serializeJson(doc, body);

        std::vector<std::pair<std::string, std::string>> headers;
        if (!job.cfg.webhookAuthHeader.empty())
            headers.push_back({"Authorization", job.cfg.webhookAuthHeader});

        return httpPost(job.cfg.webhookUrl, "application/json", body, headers, job.cfg.webhookInsecureTls, detail);
    }

    void AlertDispatcher::setStatus(uint32_t recordId, AlertChannel channel, DeliveryStatus status, const std::string &detail)
    {
        if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE)
            return;
        for (AlertRecord &record : records)
        {
            if (record.id == recordId)
            {
                const size_t index = static_cast<size_t>(channel);
                record.status[index] = status;
                record.detail[index] = detail;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }

    void AlertDispatcher::clearRecent()
    {
        if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            records.clear();
            records.shrink_to_fit();
            xSemaphoreGive(mutex);
        }
    }

    std::vector<AlertRecord> AlertDispatcher::recent() const
    {
        std::vector<AlertRecord> copy;
        if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            copy = records;
            xSemaphoreGive(mutex);
        }
        return copy;
    }

} // namespace SQM
