// EmulatedDevice: safety holds (specs/035-safety-holds), with the device's
// own rules (lib/DeviceCore DeviceHolds.cpp, lib/AlpacaLogic SafetyHolds).

#include "emulated_device.h"

using namespace DemoCore;

std::string EmulatedDevice::holdRequest(
    const std::string &method, const std::string &path, const std::string &body, const std::string &source)
{
    Core::HoldCommand command;
    if (!Core::holdCommandFromHttp(method == "DELETE", path, body, source, command))
        return response(404, errorJson("Not found"));
    const Core::HoldOutcome outcome = Core::applyHold(holds, command, nowMs, validEpoch());
    if (!outcome.changed)
        return response(outcome.status, outcome.body);
    SafetyHistory::Entry entry{};
    if (outcome.cleared)
    {
        Core::holdClearedEntry(entry, outcome.removed, safety, holds);
        pushEntry(entry);
    }
    else if (outcome.added && Core::holdSetEntry(entry, command.request.id, safety, holds))
    {
        pushEntry(entry);
    }
    if (outcome.manualChanged)
        cfg.alpaca.manualOverrideUnsafe = holds.find(Alpaca::MANUAL_HOLD_ID) != nullptr;
    return response(outcome.status, outcome.body);
}

void EmulatedDevice::tickHolds()
{
    for (const Alpaca::SafetyHold &hold : holds.update(nowMs, validEpoch()))
    {
        SafetyHistory::Entry entry{};
        Core::holdClearedEntry(entry, hold, safety, holds);
        pushEntry(entry);
        if (hold.manual())
            cfg.alpaca.manualOverrideUnsafe = false;
    }
}
