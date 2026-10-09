#pragma once

#include "Config.h"
#include "AlertEngine.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SQM
{
    class MQTTClient;

    enum class AlertChannel : uint8_t
    {
        Mqtt = 0,
        Pushover = 1,
        Ntfy = 2,
        Webhook = 3,
    };
    constexpr size_t ALERT_CHANNEL_COUNT = 4;
    constexpr uint8_t ALERT_CHANNELS_ALL = 0x0F;

    inline uint8_t alertChannelBit(AlertChannel channel)
    {
        return static_cast<uint8_t>(1u << static_cast<uint8_t>(channel));
    }
    const char *alertChannelName(AlertChannel channel);

    enum class DeliveryStatus : uint8_t
    {
        NotSent, // channel disabled / not requested
        Pending,
        Sent,
        Failed,
        Skipped, // e.g. no WiFi or an OTA update in progress
    };
    const char *deliveryStatusName(DeliveryStatus status);

    // Why each channel that's switched on can't be used right now (nullptr:
    // it can), e.g. "MQTT is off" - from the settings dependencies
    // (lib/SettingsDeps). Indexed by AlertChannel.
    using ChannelBlocks = std::array<const char *, ALERT_CHANNEL_COUNT>;

    struct AlertRecord
    {
        uint32_t id = 0;
        uint32_t uptimeSeconds = 0;
        int64_t epochSeconds = 0; // 0 if the clock wasn't set
        Alerts::Alert alert;
        DeliveryStatus status[ALERT_CHANNEL_COUNT] = {};
        std::string detail[ALERT_CHANNEL_COUNT];
    };

    // Delivers alerts. MQTT is published synchronously from the caller's
    // (main loop) task because PubSubClient isn't thread-safe; the HTTPS
    // channels go through a queue to a dedicated task so a slow TLS
    // handshake never blocks the loop or the async web server.
    class AlertDispatcher
    {
    public:
        using BusyCheck = std::function<bool()>;

        AlertDispatcher(MQTTClient *mqtt, BusyCheck networkBusy);
        ~AlertDispatcher();

        AlertDispatcher(const AlertDispatcher &) = delete;
        AlertDispatcher &operator=(const AlertDispatcher &) = delete;

        void begin();

        // Main loop task only. `channelMask` restricts delivery (used for
        // per-channel test notifications); channels disabled in `cfg` are
        // never used, test or not. A channel switched on but blocked is
        // recorded as skipped with the reason and nothing is attempted.
        void dispatch(
            const Alerts::Alert &alert,
            const AlertsConfig &cfg,
            const std::string &deviceName,
            uint8_t channelMask = ALERT_CHANNELS_ALL,
            const ChannelBlocks &blocked = ChannelBlocks{});

        std::vector<AlertRecord> recent() const;
        void clearRecent();
        // Goes up whenever recent() would answer differently (a new alert, a
        // channel's result, a clear), so the UI can fetch the list only then.
        uint32_t recentRevision() const { return revision; }

    private:
        static constexpr size_t MAX_RECORDS = 20;
        static constexpr size_t QUEUE_LENGTH = 8;
        static constexpr uint32_t TASK_STACK_WORDS = 8192;
        // The HTTPS task only exists while there's something to send: its
        // 8 KB stack is too much heap to hold for alerts that come hours apart.
        static constexpr uint32_t TASK_IDLE_EXIT_MS = 30000;

        struct Job
        {
            uint32_t recordId;
            Alerts::Alert alert;
            AlertsConfig cfg;
            std::string deviceName;
            uint8_t channelMask;
        };

        static void taskEntry(void *arg);
        void run();
        void deliver(const Job &job);
        uint32_t store(AlertRecord &record); // the record's new id, 0 if it couldn't be stored
        void queueHttp(Job *job);
        void ensureTask();
        void setStatus(uint32_t recordId, AlertChannel channel, DeliveryStatus status, const std::string &detail);

        bool sendPushover(const Job &job, std::string &detail);
        bool sendNtfy(const Job &job, std::string &detail);
        bool sendWebhook(const Job &job, std::string &detail);

        MQTTClient *mqtt;
        BusyCheck networkBusy;
        QueueHandle_t queue = nullptr;
        std::atomic<bool> taskRunning{false};
        SemaphoreHandle_t mutex = nullptr;
        std::vector<AlertRecord> records;
        uint32_t nextId = 1;
        std::atomic<uint32_t> revision{0};
    };

} // namespace SQM
