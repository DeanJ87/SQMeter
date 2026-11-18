#include "DeviceCore.h"

// Safety holds (specs/035-safety-holds): the routes' commands and answers,
// the manual hold that follows Force unsafe, and their history entries. The
// firmware (src/WebServerHolds.cpp) and the demo both run these.

namespace SQM::Core
{
    namespace
    {
        constexpr const char *HOLD_PATH = "/api/safety/hold";
        constexpr const char *CLEAR_SUFFIX = "/clear";
        constexpr const char *SETTINGS_SOURCE = "Settings";

        std::string errorBody(Alpaca::HoldError error)
        {
            StaticJsonDocument<256> doc;
            doc["error"] = Alpaca::holdErrorText(error);
            std::string out;
            serializeJson(doc, out);
            return out;
        }

        bool endsWith(const std::string &text, const std::string &suffix)
        {
            return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        HoldCommand clearCommand(const std::string &id)
        {
            HoldCommand command;
            command.kind = HoldCommand::Kind::Clear;
            command.request.id = id;
            command.error = Alpaca::validHoldId(id) ? Alpaca::HoldError::None : Alpaca::HoldError::BadId;
            return command;
        }

        HoldOutcome applyClear(Alpaca::SafetyHolds &holds, const std::string &id)
        {
            HoldOutcome outcome;
            const Alpaca::SafetyHold *hold = holds.find(id);
            if (hold == nullptr)
            {
                // Already cleared or never set: nothing changes (FR-003).
                outcome.status = 404;
                outcome.body = "{\"cleared\":false,\"reason\":\"no such hold\"}";
                return outcome;
            }
            outcome.removed = *hold;
            holds.clear(id);
            outcome.changed = true;
            outcome.cleared = true;
            outcome.manualChanged = outcome.removed.manual();
            outcome.body = "{\"cleared\":true}";
            return outcome;
        }
    } // namespace

    bool holdCommandFromHttp(
        bool isDelete, const std::string &path, const std::string &body, const std::string &querySource, HoldCommand &out)
    {
        const std::string base = HOLD_PATH;
        std::string rest = path.compare(0, base.size(), base) == 0 ? path.substr(base.size()) : std::string("?");
        if (rest == "/")
            rest.clear();
        if (rest.empty() && !isDelete)
        {
            out = HoldCommand{};
            out.error = Alpaca::parseHoldRequest(body, querySource.empty() ? "API" : querySource, out.request);
            if (out.error == Alpaca::HoldError::None && out.request.clear)
                out.kind = HoldCommand::Kind::Clear;
            return true;
        }
        if (rest.size() < 2 || rest[0] != '/')
            return false;
        rest.erase(0, 1);
        if (!isDelete)
        {
            if (!endsWith(rest, CLEAR_SUFFIX))
                return false;
            rest.resize(rest.size() - std::string(CLEAR_SUFFIX).size());
        }
        out = clearCommand(rest);
        return true;
    }

    HoldCommand holdCommandFromMqtt(const std::string &payload)
    {
        HoldCommand command;
        command.error = Alpaca::parseHoldRequest(payload, "MQTT", command.request);
        if (command.error == Alpaca::HoldError::None && command.request.clear)
            command.kind = HoldCommand::Kind::Clear;
        return command;
    }

    HoldOutcome applyHold(Alpaca::SafetyHolds &holds, const HoldCommand &command, uint32_t nowMs, int64_t epoch)
    {
        HoldOutcome outcome;
        if (command.error != Alpaca::HoldError::None)
        {
            outcome.status = 400;
            outcome.body = errorBody(command.error);
            return outcome;
        }
        if (command.kind == HoldCommand::Kind::Clear)
            return applyClear(holds, command.request.id);
        bool added = false;
        const Alpaca::HoldError error = holds.set(command.request, nowMs, epoch, &added);
        if (error != Alpaca::HoldError::None)
        {
            outcome.status = 400;
            outcome.body = errorBody(error);
            return outcome;
        }
        outcome.changed = true;
        outcome.added = added;
        outcome.manualChanged = added && command.request.id == Alpaca::MANUAL_HOLD_ID;
        StaticJsonDocument<512> doc;
        Alpaca::SafetyHolds::writeHold(doc.to<JsonObject>(), *holds.find(command.request.id), nowMs, epoch);
        serializeJson(doc, outcome.body);
        return outcome;
    }

    bool syncManualHold(Alpaca::SafetyHolds &holds, bool forceUnsafe, uint32_t nowMs, int64_t epoch)
    {
        const bool present = holds.find(Alpaca::MANUAL_HOLD_ID) != nullptr;
        if (present == forceUnsafe)
            return false;
        if (!forceUnsafe)
            return holds.clear(Alpaca::MANUAL_HOLD_ID);
        Alpaca::HoldRequest request;
        request.id = Alpaca::MANUAL_HOLD_ID;
        request.source = SETTINGS_SOURCE;
        return holds.set(request, nowMs, epoch) == Alpaca::HoldError::None;
    }

    bool holdSetEntry(SafetyHistory::Entry &entry, const std::string &id, const SafetyStatus &status, const Alpaca::SafetyHolds &holds)
    {
        if (status.isSafe || status.evaluatedAtMs == 0)
            return false;
        entry.kind = SafetyHistory::Kind::HoldSet;
        entry.flags = status.reasonFlags & ~Alpaca::UnsafeExternalHold;
        if (id == Alpaca::MANUAL_HOLD_ID)
            entry.flags |= Alpaca::UnsafeManualOverride;
        SafetyHistory::setHoldId(entry, id);
        changeHoldLabels(entry, holds);
        return true;
    }

    void holdClearedEntry(
        SafetyHistory::Entry &entry, const Alpaca::SafetyHold &cleared, const SafetyStatus &status, const Alpaca::SafetyHolds &holds)
    {
        entry.kind = SafetyHistory::Kind::HoldCleared;
        uint32_t still = status.reasonFlags & ~Alpaca::UnsafeExternalHold;
        if (cleared.manual())
            still &= ~Alpaca::UnsafeManualOverride;
        if (holds.hasExternal())
            still |= Alpaca::UnsafeExternalHold;
        entry.flags = still;
        entry.safe = still == 0;
        SafetyHistory::setHoldId(entry, cleared.id);
        // Labels: the cleared hold first (a manual hold has none), then the holds still set,
        // so the history can say which ones it is still unsafe for.
        std::vector<std::string> labels;
        if (!cleared.manual())
            labels.push_back(Alpaca::holdLabel(cleared.reason, SafetyHistory::HOLD_TEXT_BYTES - 1));
        for (const std::string &label : holds.externalLabels(SafetyHistory::HOLD_TEXT_BYTES - 1))
            labels.push_back(label);
        SafetyHistory::setHoldLabels(entry, labels);
    }

    void changeHoldLabels(SafetyHistory::Entry &entry, const Alpaca::SafetyHolds &holds)
    {
        SafetyHistory::setHoldLabels(entry, holds.externalLabels(SafetyHistory::HOLD_TEXT_BYTES - 1));
    }
} // namespace SQM::Core
