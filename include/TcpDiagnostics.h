#pragma once

#include "WebStallMonitor.h"

#include <cstdint>

namespace SQM
{
    // Reads the device's TCP state for the web-stall diagnostics
    // (lib/WebHealth decides what it means).
    namespace TcpDiagnostics
    {
        // lwIP's TCP PCBs by state, walked under the TCP/IP core lock: the
        // active, TIME-WAIT, bound and listening lists. A few dozen entries.
        WebHealth::TcpCounts count();

        // What the async_tcp task is doing, and its stack high-water mark.
        WebHealth::TaskActivity asyncTcpActivity(uint32_t &stackFree);
    } // namespace TcpDiagnostics
} // namespace SQM
