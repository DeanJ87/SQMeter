#include "SafetyHistoryLog.h"

namespace SQM
{
    namespace SafetyHistory
    {
        uint16_t startBoot(Log &log, bool keep)
        {
            if (!keep || log.magic != MAGIC || log.head >= CAPACITY || log.count > CAPACITY)
            {
                log = Log{};
                log.magic = MAGIC;
            }
            return ++log.boot;
        }

        void push(Log &log, const Entry &entry)
        {
            log.entries[log.head] = entry;
            log.head = (log.head + 1) % CAPACITY;
            if (log.count < CAPACITY)
                ++log.count;
        }

        size_t copy(const Log &log, Entry *out, size_t max)
        {
            const size_t n = log.count < max ? log.count : max;
            const size_t start = (log.head + CAPACITY - log.count) % CAPACITY;
            const size_t skip = log.count - n; // keep the newest
            for (size_t i = 0; i < n; ++i)
                out[i] = log.entries[(start + skip + i) % CAPACITY];
            return n;
        }

        bool lastAlert(const Log &log, bool &safe)
        {
            for (uint32_t i = 0; i < log.count; ++i)
            {
                const Entry &entry = log.entries[(log.head + CAPACITY - 1 - i) % CAPACITY];
                if (entry.kind == Kind::Alert)
                {
                    safe = entry.safe;
                    return true;
                }
            }
            return false;
        }

        void backfillEpochs(Entry *entries, size_t n, uint16_t boot, uint32_t epochNow, uint32_t uptimeS)
        {
            if (epochNow == 0)
                return;
            for (size_t i = 0; i < n; ++i)
                if (entries[i].epoch == 0 && entries[i].boot == boot && entries[i].uptimeS <= uptimeS)
                    entries[i].epoch = epochNow - (uptimeS - entries[i].uptimeS);
        }
    } // namespace SafetyHistory
} // namespace SQM
