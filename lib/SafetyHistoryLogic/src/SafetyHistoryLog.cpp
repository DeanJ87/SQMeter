#include "SafetyHistoryLog.h"

#include <cstring>

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

        void setHoldId(Entry &entry, const std::string &id)
        {
            std::memset(entry.holdId, 0, HOLD_ID_BYTES);
            std::memcpy(entry.holdId, id.data(), id.size() < HOLD_ID_BYTES ? id.size() : HOLD_ID_BYTES);
        }

        void setHoldLabels(Entry &entry, const std::vector<std::string> &labels)
        {
            std::string text;
            for (const std::string &label : labels)
            {
                const size_t needed = text.size() + (text.empty() ? 0 : 1) + label.size();
                if (label.empty() || needed >= HOLD_TEXT_BYTES)
                    continue;
                if (!text.empty())
                    text += HOLD_LABEL_SEPARATOR;
                text += label;
            }
            std::memset(entry.holdText, 0, HOLD_TEXT_BYTES);
            std::memcpy(entry.holdText, text.data(), text.size());
        }

        std::string holdId(const Entry &entry)
        {
            return std::string(entry.holdId, strnlen(entry.holdId, HOLD_ID_BYTES));
        }

        std::vector<std::string> holdLabels(const Entry &entry)
        {
            std::vector<std::string> labels;
            const std::string text(entry.holdText, strnlen(entry.holdText, HOLD_TEXT_BYTES));
            size_t start = 0;
            while (start < text.size())
            {
                const size_t end = text.find(HOLD_LABEL_SEPARATOR, start);
                labels.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
                if (end == std::string::npos)
                    break;
                start = end + 1;
            }
            return labels;
        }

        void writeEntry(JsonObject item, const Entry &e)
        {
            static const char *const KIND[] = {"boot", "change", "alert", "armed", "holdSet", "holdCleared"};
            const auto kind = static_cast<uint8_t>(e.kind);
            item["kind"] = KIND[kind <= static_cast<uint8_t>(Kind::HoldCleared) ? kind : 1];
            item["boot"] = e.boot;
            item["uptime"] = e.uptimeS;
            if (e.epoch != 0)
                item["timestamp"] = e.epoch;
            if (e.kind == Kind::Boot)
            {
                item["resetReason"] = e.resetReason;
                return;
            }
            item["safe"] = static_cast<bool>(e.safe);
            if (e.kind == Kind::Change)
                item["held"] = static_cast<bool>(e.held);
            if (e.kind == Kind::Change || e.kind == Kind::HoldSet || e.kind == Kind::HoldCleared)
                item["reasonFlags"] = e.flags;
            if (e.kind == Kind::HoldSet || e.kind == Kind::HoldCleared)
                item["holdId"] = holdId(e);
            const std::vector<std::string> labels = holdLabels(e);
            if (labels.empty())
                return;
            JsonArray holds = item.createNestedArray("holds");
            for (const std::string &label : labels)
                holds.add(label);
        }
    } // namespace SafetyHistory
} // namespace SQM
