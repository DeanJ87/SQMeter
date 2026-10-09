#pragma once

#include "SensorTypes.h"
#include <string>

// The RG-15's status lines ("SW ...", "Emitters ...", "PwrDays ...") and the
// one-character acks it sends on its own after a setting changes.

namespace SQM
{
    // An ack the sensor sends unprompted ('p', 'c', 'm', 'i', 'h', 'l', 's', 'x', 'y', 'o').
    bool isAsyncAck(char c);
    bool isStatusLine(const std::string &line);
    // Copies what a status line says into the diagnostics.
    void recordStatusLine(const std::string &line, RG15Diagnostics &diagnostics);
} // namespace SQM
