#include "WebHealthJson.h"

namespace SQM
{
    namespace WebHealth
    {
        void writeTcpCounts(JsonObject out, const TcpCounts &counts)
        {
            for (size_t i = 0; i < TCP_STATE_COUNT; ++i)
                if (counts.byState[i] != 0)
                    out[stateName(static_cast<TcpState>(i))] = counts.byState[i];
            out["total"] = counts.total();
            out["listenBacklog"] = counts.listenBacklog;
        }

        void writeTcp(JsonObject tcp, const TcpCounts &now, uint32_t accepted, const Peak &peak)
        {
            writeTcpCounts(tcp, now);
            tcp["accepted"] = accepted;
            JsonObject peakJson = tcp.createNestedObject("peak");
            writeTcpCounts(peakJson, peak.tcp);
            peakJson["uptimeMs"] = peak.uptimeMs;
            peakJson["epoch"] = peak.epoch;
        }

        void writeStalls(JsonObject stalls, const WebStallMonitor &monitor)
        {
            stalls["count"] = monitor.stallCount();
            stalls["ongoing"] = monitor.stalled();
            JsonArray recent = stalls.createNestedArray("recent");
            for (size_t i = 0; i < monitor.storedStalls(); ++i)
            {
                const Stall &stall = monitor.stall(i);
                JsonObject entry = recent.createNestedObject();
                entry["startEpoch"] = stall.startEpoch;
                entry["startUptimeMs"] = stall.startUptimeMs;
                entry["durationMs"] = stall.durationMs;
                entry["resumed"] = stall.resumed;
                writeTcpCounts(entry.createNestedObject("atDetection"), stall.atDetection);
                writeTcpCounts(entry.createNestedObject("worst"), stall.worst);
                entry["minFreeHeap"] = stall.minFreeHeap;
                entry["asyncTcpStackFree"] = stall.asyncTcpStackFree;
                entry["samples"] = stall.samples;
                entry["asyncTcpIdle"] = stall.asyncTcpIdle;
            }
        }
    } // namespace WebHealth
} // namespace SQM
