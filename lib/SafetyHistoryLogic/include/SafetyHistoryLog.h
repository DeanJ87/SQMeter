#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SQM
{
    namespace SafetyHistory
    {
        enum class Kind : uint8_t
        {
            Boot = 0,   // device started; resetReason says why
            Change = 1, // reported verdict changed; flags = reasons
            Alert = 2,  // a safe/unsafe alert was sent
            Armed = 3,  // alerts switched on (safe=1) or off (safe=0)
            // Safety holds (spec 035 FR-009): flags = the other reasons it was
            // (still) unsafe for, holdLabels = the external holds' labels.
            HoldSet = 4,     // a hold set while already unsafe (else the Change records it)
            HoldCleared = 5, // a hold cleared or lifted at its time
        };

        constexpr size_t HOLD_ID_BYTES = 24;   // a hold id, not NUL-terminated at full length
        constexpr size_t HOLD_TEXT_BYTES = 40; // labels separated by HOLD_LABEL_SEPARATOR, NUL-terminated
        constexpr char HOLD_LABEL_SEPARATOR = '\x1f';

        struct Entry
        {
            uint32_t epoch;   // 0 if the clock wasn't set (filled in later where possible)
            uint32_t uptimeS; // seconds since that boot
            uint32_t flags;   // UnsafeReasonFlag bits at the time
            uint16_t boot;    // which boot it belongs to
            Kind kind;
            uint8_t safe : 1;
            uint8_t held : 1; // unsafe only because of the safe delay
            uint8_t resetReason : 6;
            char holdId[HOLD_ID_BYTES];     // HoldSet/HoldCleared: which hold
            char holdText[HOLD_TEXT_BYTES]; // Change/HoldSet: every external hold; HoldCleared: that one
        };

        constexpr size_t CAPACITY = 32;
        constexpr uint32_t MAGIC = 0x53484932; // "SHI2" (spec 035 added the hold fields)

        // The ring buffer itself - plain data so it can live in RTC memory,
        // which keeps whatever was there before a restart (garbage after a
        // power cut). The functions below are the logic, without locking.
        struct Log
        {
            uint32_t magic;
            uint32_t head; // next slot to write
            uint32_t count;
            uint16_t boot;
            Entry entries[CAPACITY];
        };

        // Starts a new boot: keeps the log if it's intact and the reset kept
        // RTC memory (`keep`), else clears it. Returns the new boot number.
        uint16_t startBoot(Log &log, bool keep);
        void push(Log &log, const Entry &entry);
        // Oldest first, the newest `max`; returns how many were written.
        size_t copy(const Log &log, Entry *out, size_t max);
        // The newest safe/unsafe alert sent, from any boot; false if none.
        bool lastAlert(const Log &log, bool &safe);
        // Dates entries from boot `boot` made before the clock was set, from
        // their uptime, now that it's `epochNow` at uptime `uptimeS`.
        void backfillEpochs(Entry *entries, size_t n, uint16_t boot, uint32_t epochNow, uint32_t uptimeS);

        // An entry's hold id, and its hold labels: whole labels only, as many as fit.
        void setHoldId(Entry &entry, const std::string &id);
        void setHoldLabels(Entry &entry, const std::vector<std::string> &labels);
        std::string holdId(const Entry &entry);
        std::vector<std::string> holdLabels(const Entry &entry);
        // One /api/safety/history entry.
        void writeEntry(JsonObject item, const Entry &entry);
    } // namespace SafetyHistory
} // namespace SQM
