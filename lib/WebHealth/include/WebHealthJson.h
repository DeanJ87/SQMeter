#pragma once

#include "WebStallMonitor.h"

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>

// /api/status connections.tcp and connections.stalls (not in the /ws/status
// pushes: nothing in the web UI reads them).

namespace SQM
{
    namespace WebHealth
    {
        // Extra ArduinoJson pool for both sections at their largest (every
        // state in use, a full stall history), with 20% headroom; pinned by
        // test_web_health (test_documents_fit_their_pool).
        constexpr size_t DOC_CAPACITY = 6656;

        // Counts by state (states with no PCBs left out), total, listenBacklog.
        void writeTcpCounts(JsonObject out, const TcpCounts &counts);

        // connections.tcp: the counts now, connections accepted since boot,
        // and the peak since boot.
        void writeTcp(JsonObject tcp, const TcpCounts &now, uint32_t accepted, const Peak &peak);

        // connections.stalls: count since boot, ongoing, and the recent ones (newest first).
        void writeStalls(JsonObject stalls, const WebStallMonitor &monitor);
    } // namespace WebHealth
} // namespace SQM
