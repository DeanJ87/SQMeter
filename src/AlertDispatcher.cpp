#include "AlertDispatcher.h"
#include "AlertDelivery.h"
#include "AlertRootCA.h"
#include "Logger.h"
#include "MQTTClient.h"
#include "TlsLock.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "DualStackClient.h"
#include "NetAddress.h"
#include <ctime>
#include <memory>

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "Alerts";
        constexpr int HTTP_TIMEOUT_MS = 10000;
        constexpr size_t MAX_ERROR_BODY_CHARS = 120;

        int64_t epochNow()
        {
            const time_t now = time(nullptr);
            return now >= 1704067200 ? static_cast<int64_t>(now) : 0;
        }

        // Plain http also reaches IPv6 literals and IPv6-only names (spec 015).
        std::unique_ptr<WiFiClient> clientFor(const std::string &url, bool insecureTls, const char *rootCa)
        {
            if (url.rfind("https://", 0) != 0)
                return std::make_unique<DualStackClient>();
            auto secure = std::make_unique<WiFiClientSecure>();
            if (insecureTls)
                secure->setInsecure();
            else
                secure->setCACert(rootCa);
            return secure;
        }

        // HTTPClient splits "http://[fd00::10]:8080/x" at the first ':', so IPv6
        // hosts go in by parts (the bracketed host is the Host header).
        bool beginRequest(HTTPClient &http, WiFiClient &client, const std::string &url)
        {
            Net::HttpUrl parsed;
            if (Net::parseHttpUrl(url, parsed) == Net::UrlError::None && parsed.host.ipv6)
                return http.begin(client, Net::hostForUrl(parsed.host).c_str(), parsed.port, parsed.path.c_str(), parsed.https);
            return http.begin(client, url.c_str());
        }

        // Which certificates an HTTPS request trusts.
        struct TlsTrust
        {
            bool insecure = false; // the webhook's "skip certificate check"
            const char *rootCa = ALERT_ROOT_CA_PEM;
        };

        // A failed reply's text. Pushover (and ntfy) explain failures in
        // JSON: show the message itself rather than truncated raw JSON.
        std::string errorBody(HTTPClient &http)
        {
            String response = http.getString();
            StaticJsonDocument<512> errorDoc;
            if (!deserializeJson(errorDoc, response))
            {
                const char *message = errorDoc["errors"][0] | errorDoc["error"] | static_cast<const char *>(nullptr);
                if (message != nullptr)
                    response = message;
            }
            return response.length() > 0 ? ": " + std::string(response.substring(0, MAX_ERROR_BODY_CHARS).c_str()) : std::string();
        }

        bool httpPost(const Delivery::HttpRequest &request, const TlsTrust &tls, std::string &detail)
        {
            const std::unique_ptr<WiFiClient> client = clientFor(request.url, tls.insecure, tls.rootCa);
            HTTPClient http;
            http.setTimeout(HTTP_TIMEOUT_MS);
            http.setConnectTimeout(HTTP_TIMEOUT_MS);
            if (!beginRequest(http, *client, request.url))
            {
                detail = "Invalid URL";
                return false;
            }
            http.addHeader("Content-Type", request.contentType.c_str());
            for (const auto &header : request.headers)
                http.addHeader(header.first.c_str(), header.second.c_str());

            const std::string &body = request.body;
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
                detail += errorBody(http);
            http.end();
            return ok;
        }

        void wantedChannels(const AlertsConfig &cfg, uint8_t channelMask, bool (&wanted)[ALERT_CHANNEL_COUNT])
        {
            wanted[0] = cfg.mqttEnabled && (channelMask & alertChannelBit(AlertChannel::Mqtt));
            wanted[1] = cfg.pushoverEnabled && (channelMask & alertChannelBit(AlertChannel::Pushover));
            wanted[2] = cfg.ntfyEnabled && (channelMask & alertChannelBit(AlertChannel::Ntfy));
            wanted[3] = cfg.webhookEnabled && (channelMask & alertChannelBit(AlertChannel::Webhook));
        }

        // Switched on but inactive (e.g. MQTT alerts with MQTT off): skipped, never "failed".
        void skipBlocked(AlertRecord &record, const ChannelBlocks &blocked, bool (&wanted)[ALERT_CHANNEL_COUNT])
        {
            for (size_t i = 0; i < ALERT_CHANNEL_COUNT; ++i)
            {
                if (!wanted[i] || blocked[i] == nullptr)
                    continue;
                wanted[i] = false;
                record.status[i] = DeliveryStatus::Skipped;
                record.detail[i] = blocked[i];
            }
        }

        // The HTTP channels (everything but MQTT) still to send, marked
        // pending; returns their bits.
        uint8_t markPending(AlertRecord &record, const bool (&wanted)[ALERT_CHANNEL_COUNT])
        {
            uint8_t httpMask = 0;
            for (size_t i = 1; i < ALERT_CHANNEL_COUNT; ++i)
            {
                if (!wanted[i])
                    continue;
                record.status[i] = DeliveryStatus::Pending;
                httpMask |= static_cast<uint8_t>(1u << i);
            }
            return httpMask;
        }
    } // namespace

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
        : mqtt(mqttClient),
          networkBusy(std::move(busy))
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

    void AlertDispatcher::dispatch(
        const Alerts::Alert &alert,
        const AlertsConfig &cfg,
        const std::string &deviceName,
        uint8_t channelMask,
        const ChannelBlocks &blocked)
    {
        AlertRecord record;
        record.uptimeSeconds = millis() / 1000;
        record.epochSeconds = epochNow();
        record.alert = alert;

        bool wanted[ALERT_CHANNEL_COUNT];
        wantedChannels(cfg, channelMask, wanted);
        skipBlocked(record, blocked, wanted);

        // MQTT: publish right here on the main loop task.
        if (wanted[0])
        {
            const std::string payload = Delivery::alertJson(alert, deviceName, record.epochSeconds);
            const bool ok = mqtt != nullptr && mqtt->publishSubtopic("alerts", payload, false);
            record.status[0] = ok ? DeliveryStatus::Sent : DeliveryStatus::Failed;
            record.detail[0] = ok ? "Published" : "MQTT not connected";
        }

        const uint8_t httpMask = markPending(record, wanted);

        const uint32_t id = store(record);
        Logger::info(TAG, "%s: %s", alert.title.c_str(), alert.message.c_str());
        if (httpMask != 0 && queue != nullptr && id != 0)
            queueHttp(new Job{id, alert, cfg, deviceName, httpMask});
    }

    uint32_t AlertDispatcher::store(AlertRecord &record)
    {
        if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE)
            return 0;
        record.id = nextId++;
        records.push_back(record);
        if (records.size() > MAX_RECORDS)
            records.erase(records.begin());
        xSemaphoreGive(mutex);
        return record.id;
    }

    // HTTPS channels go to the delivery task; a full queue fails them at once.
    void AlertDispatcher::queueHttp(Job *job)
    {
        if (xQueueSend(queue, &job, 0) == pdTRUE)
        {
            ensureTask();
            return;
        }
        Logger::warn(TAG, "Alert queue full - dropping notification");
        for (size_t i = 1; i < ALERT_CHANNEL_COUNT; ++i)
            if (job->channelMask & (1u << i))
                setStatus(job->recordId, static_cast<AlertChannel>(i), DeliveryStatus::Failed, "Queue full");
        delete job;
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
        const Delivery::HttpRequest request = Delivery::pushoverRequest(
            job.alert, {job.cfg.pushoverUserKey, job.cfg.pushoverAppToken, job.cfg.pushoverSound}, job.deviceName);
        return httpPost(request, TlsTrust{false, PUSHOVER_ROOT_CA_PEM}, detail);
    }

    bool AlertDispatcher::sendNtfy(const Job &job, std::string &detail)
    {
        const Delivery::HttpRequest request =
            Delivery::ntfyRequest(job.alert, {job.cfg.ntfyServer, job.cfg.ntfyTopic, job.cfg.ntfyToken}, job.deviceName);
        const bool ntfySh = request.url.rfind("https://ntfy.sh/", 0) == 0;
        return httpPost(request, TlsTrust{false, ntfySh ? NTFY_SH_ROOT_CA_PEM : ALERT_ROOT_CA_PEM}, detail);
    }

    bool AlertDispatcher::sendWebhook(const Job &job, std::string &detail)
    {
        Delivery::HttpRequest request;
        request.url = job.cfg.webhookUrl;
        request.contentType = "application/json";
        request.body = Delivery::alertJson(job.alert, job.deviceName, epochNow());
        if (!job.cfg.webhookAuthHeader.empty())
            request.headers.push_back({"Authorization", job.cfg.webhookAuthHeader});
        return httpPost(request, TlsTrust{job.cfg.webhookInsecureTls, ALERT_ROOT_CA_PEM}, detail);
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
