#include "SafetyHistory.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <time.h>

namespace SQM
{
    namespace SafetyHistory
    {
        namespace
        {
            constexpr uint32_t MAGIC = 0x53484931; // "SHI1"
            constexpr time_t CLOCK_VALID = 1704067200;

            struct Store
            {
                uint32_t magic;
                uint32_t head; // next slot to write
                uint32_t count;
                uint16_t boot;
                Entry entries[CAPACITY];
            };

            RTC_NOINIT_ATTR Store store;
            portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

            uint32_t epochNow()
            {
                const time_t now = time(nullptr);
                return now >= CLOCK_VALID ? static_cast<uint32_t>(now) : 0;
            }

            void push(const Entry &entry)
            {
                portENTER_CRITICAL(&lock);
                store.entries[store.head] = entry;
                store.head = (store.head + 1) % CAPACITY;
                if (store.count < CAPACITY)
                    ++store.count;
                portEXIT_CRITICAL(&lock);
            }

            Entry make(Kind kind)
            {
                Entry entry{};
                entry.epoch = epochNow();
                entry.uptimeS = millis() / 1000;
                entry.boot = store.boot;
                entry.kind = kind;
                return entry;
            }
        } // namespace

        void begin(uint8_t resetReason)
        {
            // RTC memory is garbage after power-on or a brownout.
            if (store.magic != MAGIC || store.head >= CAPACITY || store.count > CAPACITY || resetReason == ESP_RST_POWERON ||
                resetReason == ESP_RST_BROWNOUT)
            {
                store = Store{};
                store.magic = MAGIC;
            }
            ++store.boot;
            Entry entry = make(Kind::Boot);
            entry.resetReason = resetReason & 0x3F;
            push(entry);
        }

        void recordChange(bool safe, bool held, uint32_t flags)
        {
            Entry entry = make(Kind::Change);
            entry.safe = safe;
            entry.held = held;
            entry.flags = flags;
            push(entry);
        }

        void recordAlert(bool safe)
        {
            Entry entry = make(Kind::Alert);
            entry.safe = safe;
            push(entry);
        }

        void recordArmed(bool armed)
        {
            Entry entry = make(Kind::Armed);
            entry.safe = armed;
            push(entry);
        }

        size_t entries(Entry *out, size_t max)
        {
            portENTER_CRITICAL(&lock);
            const size_t n = store.count < max ? store.count : max;
            const size_t start = (store.head + CAPACITY - store.count) % CAPACITY;
            const size_t skip = store.count - n; // keep the newest
            for (size_t i = 0; i < n; ++i)
                out[i] = store.entries[(start + skip + i) % CAPACITY];
            portEXIT_CRITICAL(&lock);

            // Entries from this boot made before the clock was set can be
            // dated now from their uptime.
            const uint32_t now = epochNow();
            const uint32_t uptime = millis() / 1000;
            for (size_t i = 0; i < n; ++i)
                if (out[i].epoch == 0 && now != 0 && out[i].boot == store.boot && out[i].uptimeS <= uptime)
                    out[i].epoch = now - (uptime - out[i].uptimeS);
            return n;
        }

        uint16_t currentBoot() { return store.boot; }

        bool lastAlert(bool &safe)
        {
            bool found = false;
            portENTER_CRITICAL(&lock);
            for (uint32_t i = 0; i < store.count && !found; ++i)
            {
                const Entry &entry = store.entries[(store.head + CAPACITY - 1 - i) % CAPACITY];
                if (entry.kind == Kind::Alert)
                {
                    safe = entry.safe;
                    found = true;
                }
            }
            portEXIT_CRITICAL(&lock);
            return found;
        }
    } // namespace SafetyHistory
} // namespace SQM
