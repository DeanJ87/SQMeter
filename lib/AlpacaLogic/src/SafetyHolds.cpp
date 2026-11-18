#include "SafetyHolds.h"

#include <algorithm>

namespace SQM
{
    namespace Alpaca
    {
        namespace
        {
            // Room for one request with every field at its longest (multi-byte reason).
            constexpr size_t REQUEST_DOC_CAPACITY = 1024;
            constexpr size_t STORE_DOC_CAPACITY = 4096;

            bool clockValid(int64_t epoch)
            {
                return epoch >= HOLD_CLOCK_VALID_EPOCH;
            }

            // Characters (UTF-8 code points) in text, or SIZE_MAX if it has a control character.
            size_t printableChars(const std::string &text)
            {
                size_t chars = 0;
                for (const char c : text)
                {
                    const auto byte = static_cast<unsigned char>(c);
                    if (byte < 0x20 || byte == 0x7F)
                        return SIZE_MAX;
                    if ((byte & 0xC0) != 0x80)
                        ++chars;
                }
                return chars;
            }

            uint32_t remainingMs(const SafetyHold &hold, uint32_t nowMs, int64_t epoch)
            {
                const uint32_t total = hold.durationS * 1000U;
                const uint32_t elapsed = nowMs - hold.startMs;
                uint32_t left = elapsed >= total ? 0 : total - elapsed;
                if (clockValid(epoch) && hold.expiresEpoch != 0)
                {
                    const int64_t byClock = hold.expiresEpoch > epoch ? (hold.expiresEpoch - epoch) * 1000 : 0;
                    left = std::min<int64_t>(left, byClock);
                }
                return left;
            }
        } // namespace

        const char *holdErrorText(HoldError error)
        {
            switch (error)
            {
            case HoldError::None:
                return "";
            case HoldError::BadJson:
                return "Send JSON: {\"id\": \"smoke\", \"reason\": \"...\"}";
            case HoldError::BadId:
                return "id must be 1-24 characters a-z, 0-9, _ or -";
            case HoldError::BadReason:
                return "reason must be 1-96 characters, no control characters";
            case HoldError::BadSource:
                return "source must be up to 32 characters, no control characters";
            case HoldError::BadExpiry:
                return "expiresMin must be a whole number of minutes, 1-10080";
            case HoldError::Full:
                return "8 holds already: clear one first";
            }
            return "Invalid hold";
        }

        bool validHoldId(const std::string &id)
        {
            if (id.empty() || id.size() > HOLD_ID_MAX_CHARS)
                return false;
            return std::all_of(
                id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
        }

        HoldError validateHoldRequest(HoldRequest &request)
        {
            if (!validHoldId(request.id))
                return HoldError::BadId;
            if (request.id == MANUAL_HOLD_ID)
                request.reason = MANUAL_HOLD_REASON;
            const size_t reasonChars = printableChars(request.reason);
            if (reasonChars == 0 || reasonChars > HOLD_REASON_MAX_CHARS)
                return HoldError::BadReason;
            const size_t sourceChars = printableChars(request.source);
            if (sourceChars == 0 || sourceChars > HOLD_SOURCE_MAX_CHARS)
                return HoldError::BadSource;
            if (request.expiresMin > HOLD_EXPIRES_MAX_MINUTES)
                return HoldError::BadExpiry;
            return HoldError::None;
        }

        HoldError parseHoldRequest(const std::string &json, const std::string &defaultSource, HoldRequest &out)
        {
            StaticJsonDocument<REQUEST_DOC_CAPACITY> doc;
            if (deserializeJson(doc, json) != DeserializationError::Ok || !doc.is<JsonObject>())
                return HoldError::BadJson;
            const JsonObject root = doc.as<JsonObject>();
            if (!root["id"].is<const char *>())
                return HoldError::BadId;
            out.id = root["id"].as<const char *>();
            out.clear = root["clear"] | false;
            if (out.clear)
                return validHoldId(out.id) ? HoldError::None : HoldError::BadId;
            out.reason = root["reason"] | "";
            out.source = root["source"] | "";
            if (out.source.empty())
                out.source = defaultSource;
            const JsonVariantConst expires = root["expiresMin"];
            out.expiresMin = 0;
            if (!expires.isNull())
            {
                if (!expires.is<uint32_t>() || expires.as<uint32_t>() == 0)
                    return HoldError::BadExpiry;
                out.expiresMin = expires.as<uint32_t>();
            }
            return validateHoldRequest(out);
        }

        std::string holdLabel(const std::string &reason, size_t maxBytes)
        {
            std::string label = reason.substr(0, reason.find(':'));
            while (!label.empty() && label.back() == ' ')
                label.pop_back();
            if (label.empty())
                label = reason;
            if (label.size() <= maxBytes)
                return label;
            size_t cut = maxBytes;
            while (cut > 0 && (static_cast<unsigned char>(label[cut]) & 0xC0) == 0x80)
                --cut; // don't split a character
            return label.substr(0, cut);
        }

        HoldError SafetyHolds::set(HoldRequest request, uint32_t nowMs, int64_t epoch, bool *added)
        {
            const HoldError error = validateHoldRequest(request);
            if (error != HoldError::None)
                return error;
            auto existing = std::find_if(holds.begin(), holds.end(), [&](const SafetyHold &h) { return h.id == request.id; });
            if (existing == holds.end() && holds.size() >= MAX_HOLDS)
                return HoldError::Full;
            if (added != nullptr)
                *added = existing == holds.end();
            if (existing == holds.end())
            {
                SafetyHold hold;
                hold.id = request.id;
                hold.sinceEpoch = clockValid(epoch) ? epoch : 0;
                hold.sinceMs = nowMs;
                hold.setThisBoot = true;
                holds.push_back(hold);
                existing = holds.end() - 1;
            }
            existing->reason = request.reason;
            existing->source = request.source;
            existing->durationS = request.expiresMin * 60U;
            existing->startMs = nowMs;
            existing->expiresEpoch = existing->durationS != 0 && clockValid(epoch) ? epoch + existing->durationS : 0;
            return HoldError::None;
        }

        bool SafetyHolds::clear(const std::string &id)
        {
            const auto it = std::find_if(holds.begin(), holds.end(), [&](const SafetyHold &h) { return h.id == id; });
            if (it == holds.end())
                return false;
            holds.erase(it);
            return true;
        }

        const SafetyHold *SafetyHolds::find(const std::string &id) const
        {
            const auto it = std::find_if(holds.begin(), holds.end(), [&](const SafetyHold &h) { return h.id == id; });
            return it == holds.end() ? nullptr : &*it;
        }

        bool SafetyHolds::hasExternal() const
        {
            return std::any_of(holds.begin(), holds.end(), [](const SafetyHold &h) { return !h.manual(); });
        }

        std::vector<SafetyHold> SafetyHolds::update(uint32_t nowMs, int64_t epoch)
        {
            std::vector<SafetyHold> lifted;
            for (auto it = holds.begin(); it != holds.end();)
            {
                if (clockValid(epoch) && it->sinceEpoch == 0 && it->setThisBoot)
                    it->sinceEpoch = epoch - static_cast<int64_t>((nowMs - it->sinceMs) / 1000);
                if (it->durationS == 0)
                {
                    ++it;
                    continue;
                }
                const uint32_t left = remainingMs(*it, nowMs, epoch);
                if (clockValid(epoch) && it->expiresEpoch == 0)
                    it->expiresEpoch = epoch + (left + 999) / 1000; // shown; not stored (FR-007)
                if (left != 0)
                {
                    ++it;
                    continue;
                }
                lifted.push_back(*it);
                it = holds.erase(it);
            }
            return lifted;
        }

        std::vector<std::string> SafetyHolds::externalReasons() const
        {
            std::vector<std::string> out;
            for (const SafetyHold &h : holds)
                if (!h.manual())
                    out.push_back(h.reason);
            return out;
        }

        std::vector<std::string> SafetyHolds::externalLabels(size_t maxBytes) const
        {
            std::vector<std::string> out;
            for (const SafetyHold &h : holds)
                if (!h.manual())
                    out.push_back(holdLabel(h.reason, maxBytes));
            return out;
        }

        std::string SafetyHolds::serialize() const
        {
            DynamicJsonDocument doc(STORE_DOC_CAPACITY);
            JsonArray list = doc.createNestedArray("h");
            for (const SafetyHold &h : holds)
            {
                JsonObject item = list.createNestedObject();
                item["i"] = h.id.c_str();
                item["r"] = h.reason.c_str();
                item["s"] = h.source.c_str();
                if (h.sinceEpoch != 0)
                    item["t"] = h.sinceEpoch;
                if (h.durationS != 0)
                    item["d"] = h.durationS;
                if (h.expiresEpoch != 0)
                    item["e"] = h.expiresEpoch;
            }
            std::string out;
            serializeJson(doc, out);
            return out;
        }

        bool SafetyHolds::restore(const std::string &stored, uint32_t nowMs)
        {
            holds.clear();
            DynamicJsonDocument doc(STORE_DOC_CAPACITY);
            if (deserializeJson(doc, stored) != DeserializationError::Ok)
                return false;
            for (JsonObjectConst item : doc["h"].as<JsonArrayConst>())
            {
                HoldRequest request;
                request.id = item["i"] | "";
                request.reason = item["r"] | "";
                request.source = item["s"] | "";
                const uint32_t durationS = item["d"] | 0U;
                request.expiresMin = (durationS + 59) / 60;
                if (validateHoldRequest(request) != HoldError::None || find(request.id) != nullptr || holds.size() >= MAX_HOLDS)
                    continue;
                SafetyHold hold;
                hold.id = request.id;
                hold.reason = request.reason;
                hold.source = request.source;
                hold.sinceEpoch = item["t"] | static_cast<int64_t>(0);
                hold.sinceMs = nowMs;
                hold.durationS = durationS;
                hold.expiresEpoch = item["e"] | static_cast<int64_t>(0);
                hold.startMs = nowMs; // a full duration from boot: never lifts early without a clock
                holds.push_back(hold);
            }
            return true;
        }

        void SafetyHolds::writeJson(JsonArray out, uint32_t nowMs, int64_t epoch) const
        {
            for (const SafetyHold &h : holds)
                writeHold(out.createNestedObject(), h, nowMs, epoch);
        }

        void SafetyHolds::writeHold(JsonObject item, const SafetyHold &h, uint32_t nowMs, int64_t epoch)
        {
            item["id"] = h.id;
            item["reason"] = h.reason;
            item["source"] = h.source;
            if (h.sinceEpoch != 0)
                item["sinceEpoch"] = h.sinceEpoch;
            else if (h.setThisBoot)
                item["sinceAgeMs"] = nowMs - h.sinceMs;
            if (h.durationS == 0)
                return;
            if (h.expiresEpoch != 0)
                item["expiresEpoch"] = h.expiresEpoch;
            item["expiresInMs"] = remainingMs(h, nowMs, epoch);
        }

    } // namespace Alpaca
} // namespace SQM
