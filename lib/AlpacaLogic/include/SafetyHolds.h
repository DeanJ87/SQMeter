#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SQM
{
    namespace Alpaca
    {
        // Safety holds (specs/035-safety-holds): named events another system
        // (Home Assistant, a roof controller) sets to keep the SafetyMonitor
        // unsafe, and later clears - that event only. The Force unsafe
        // setting is the built-in hold "manual".
        constexpr size_t MAX_HOLDS = 8;
        constexpr size_t HOLD_ID_MAX_CHARS = 24;
        constexpr size_t HOLD_REASON_MAX_CHARS = 96;
        constexpr size_t HOLD_SOURCE_MAX_CHARS = 32;
        constexpr uint32_t HOLD_EXPIRES_MAX_MINUTES = 10080; // a week
        constexpr const char *MANUAL_HOLD_ID = "manual";
        // The reason Force unsafe has always given (the device message catalogue translates it).
        constexpr const char *MANUAL_HOLD_REASON = "Manual override forces unsafe";
        // A wall clock before this (2024-01-01) means "not set yet".
        constexpr int64_t HOLD_CLOCK_VALID_EPOCH = 1704067200;

        // One hold. Times: sinceEpoch/expiresEpoch are Unix seconds (0 =
        // unknown); sinceMs/startMs are the device clock (millis()).
        struct SafetyHold
        {
            std::string id;
            std::string reason;
            std::string source;
            int64_t sinceEpoch = 0;
            uint32_t sinceMs = 0;
            bool setThisBoot = false; // sinceMs is when it was set (else it was restored at boot)
            uint32_t durationS = 0;   // 0 = until cleared
            int64_t expiresEpoch = 0; // when it lifts by the wall clock, 0 = not known
            uint32_t startMs = 0;     // the duration counts from here: set, or boot after a restore

            bool manual() const { return id == MANUAL_HOLD_ID; }
        };

        // A set request: POST /api/safety/hold or MQTT <base>/safety/hold/set.
        struct HoldRequest
        {
            std::string id;
            std::string reason;
            std::string source;
            uint32_t expiresMin = 0; // 0 = until cleared
            bool clear = false;      // MQTT {"id", "clear": true}
        };

        enum class HoldError : uint8_t
        {
            None,
            BadJson,
            BadId,
            BadReason,
            BadSource,
            BadExpiry,
            Full,
        };
        // English text for an error (the HTTP 400 body, the MQTT log line).
        const char *holdErrorText(HoldError error);

        // [a-z0-9_-]{1,24}
        bool validHoldId(const std::string &id);
        // Parses {"id","reason","source"?,"expiresMin"?,"clear"?} and checks it
        // (a clear only needs a valid id). An empty source becomes defaultSource.
        HoldError parseHoldRequest(const std::string &json, const std::string &defaultSource, HoldRequest &out);
        // Checks a set request; the manual hold's reason is always MANUAL_HOLD_REASON.
        HoldError validateHoldRequest(HoldRequest &request);

        // The short name the safety history keeps: the reason up to its first
        // colon ("Smoke: neighbour's fire" -> "Smoke"), at most maxBytes,
        // cut on a UTF-8 character boundary.
        std::string holdLabel(const std::string &reason, size_t maxBytes);

        // The holds in effect, oldest first. Not thread-safe: the firmware
        // locks around it (src/WebServerHolds.cpp).
        class SafetyHolds
        {
        public:
            // Adds a hold, or updates one with the same id (reason, source and
            // expiry replaced, `since` kept). `added` says which.
            HoldError set(HoldRequest request, uint32_t nowMs, int64_t epoch, bool *added = nullptr);
            // false (and nothing changes) when there is no such hold.
            bool clear(const std::string &id);
            const SafetyHold *find(const std::string &id) const;
            const std::vector<SafetyHold> &all() const { return holds; }
            bool hasExternal() const;

            // Once per evaluation: dates holds set before the clock was set,
            // and lifts the holds whose time has come (returned, oldest first).
            // A hold lifts at its wall-clock time, or once its full duration
            // has passed since it was set or restored - never early.
            std::vector<SafetyHold> update(uint32_t nowMs, int64_t epoch);

            // The external (not manual) holds' reasons and labels.
            std::vector<std::string> externalReasons() const;
            std::vector<std::string> externalLabels(size_t maxBytes) const;

            // The NVS form, and back (restore drops entries that don't validate).
            std::string serialize() const;
            bool restore(const std::string &stored, uint32_t nowMs);

            // /api/safety "holds": {id, reason, source, sinceEpoch | sinceAgeMs,
            // expiresEpoch?, expiresInMs?}.
            void writeJson(JsonArray out, uint32_t nowMs, int64_t epoch) const;
            // One of them (also the answer to a set).
            static void writeHold(JsonObject item, const SafetyHold &hold, uint32_t nowMs, int64_t epoch);

        private:
            std::vector<SafetyHold> holds;
        };

    } // namespace Alpaca
} // namespace SQM
