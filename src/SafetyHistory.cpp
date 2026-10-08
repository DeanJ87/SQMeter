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
            constexpr time_t CLOCK_VALID = 1704067200;

            RTC_NOINIT_ATTR Log store;
            portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

            uint32_t epochNow()
            {
                const time_t now = time(nullptr);
                return now >= CLOCK_VALID ? static_cast<uint32_t>(now) : 0;
            }

            void record(const Entry &entry)
            {
                portENTER_CRITICAL(&lock);
                push(store, entry);
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
            startBoot(store, resetReason != ESP_RST_POWERON && resetReason != ESP_RST_BROWNOUT);
            Entry entry = make(Kind::Boot);
            entry.resetReason = resetReason & 0x3F;
            record(entry);
        }

        void recordChange(bool safe, bool held, uint32_t flags)
        {
            Entry entry = make(Kind::Change);
            entry.safe = safe;
            entry.held = held;
            entry.flags = flags;
            record(entry);
        }

        void recordAlert(bool safe)
        {
            Entry entry = make(Kind::Alert);
            entry.safe = safe;
            record(entry);
        }

        void recordArmed(bool armed)
        {
            Entry entry = make(Kind::Armed);
            entry.safe = armed;
            record(entry);
        }

        size_t entries(Entry *out, size_t max)
        {
            portENTER_CRITICAL(&lock);
            const size_t n = copy(store, out, max);
            portEXIT_CRITICAL(&lock);
            backfillEpochs(out, n, store.boot, epochNow(), millis() / 1000);
            return n;
        }

        uint16_t currentBoot() { return store.boot; }

        bool lastAlert(bool &safe)
        {
            portENTER_CRITICAL(&lock);
            const bool found = SafetyHistory::lastAlert(store, safe);
            portEXIT_CRITICAL(&lock);
            return found;
        }
    } // namespace SafetyHistory
} // namespace SQM
