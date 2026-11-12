#include "WebStallMonitor.h"

namespace SQM
{
    namespace WebHealth
    {
        namespace
        {
            constexpr const char *STATE_NAMES[TCP_STATE_COUNT] = {
                "closed",
                "listen",
                "synSent",
                "synRcvd",
                "established",
                "finWait1",
                "finWait2",
                "closeWait",
                "closing",
                "lastAck",
                "timeWait",
            };
            constexpr uint8_t COUNT_MAX = 255;

            uint32_t minimum(uint32_t a, uint32_t b)
            {
                return a < b ? a : b;
            }
        } // namespace

        const char *stateName(TcpState state)
        {
            const size_t index = static_cast<size_t>(state);
            return index < TCP_STATE_COUNT ? STATE_NAMES[index] : "unknown";
        }

        void TcpCounts::add(uint8_t state)
        {
            if (state < TCP_STATE_COUNT && byState[state] < COUNT_MAX)
                ++byState[state];
        }

        size_t TcpCounts::total() const
        {
            size_t sum = 0;
            for (size_t i = 0; i < TCP_STATE_COUNT; ++i)
                if (i != static_cast<size_t>(TcpState::Listen))
                    sum += byState[i];
            return sum;
        }

        const Stall &WebStallMonitor::stall(size_t index) const
        {
            if (index >= stored)
                return history[0];
            return history[(head + STALL_HISTORY - 1 - index) % STALL_HISTORY];
        }

        void WebStallMonitor::update(const Sample &sample)
        {
            updatePeak(sample);
            if (!started)
            {
                started = true;
                lastAccepted = sample.accepted;
                lastAcceptMs = sample.nowMs;
                lastAlpacaRequests = sample.alpacaRequests;
                return;
            }
            if (sample.alpacaRequests != lastAlpacaRequests)
            {
                lastAlpacaRequests = sample.alpacaRequests;
                lastAlpacaMs = sample.nowMs;
                alpacaSeen = true;
            }
            if (sample.accepted != lastAccepted)
            {
                if (inStall)
                {
                    ongoing.durationMs = sample.nowMs - ongoing.startUptimeMs;
                    endStall(true);
                }
                gaveUp = false;
                lastAccepted = sample.accepted;
                lastAcceptMs = sample.nowMs;
                return;
            }
            if (inStall)
                extendStall(sample);
            else if (!gaveUp && sample.nowMs - lastAcceptMs >= STALL_AFTER_MS && clientPolling(sample.nowMs))
                beginStall(sample);
        }

        void WebStallMonitor::updatePeak(const Sample &sample)
        {
            if (sample.tcp.total() <= peakSeen.tcp.total())
                return;
            peakSeen.tcp = sample.tcp;
            peakSeen.uptimeMs = sample.nowMs;
            peakSeen.epoch = sample.epoch;
        }

        bool WebStallMonitor::clientPolling(uint32_t nowMs) const
        {
            return alpacaSeen && nowMs - lastAlpacaMs < POLLING_WINDOW_MS;
        }

        void WebStallMonitor::beginStall(const Sample &sample)
        {
            const uint32_t gapMs = sample.nowMs - lastAcceptMs;
            ongoing = Stall{};
            ongoing.startUptimeMs = lastAcceptMs;
            ongoing.startEpoch = sample.epoch != 0 && sample.epoch > gapMs / 1000 ? sample.epoch - gapMs / 1000 : 0;
            ongoing.atDetection = sample.tcp;
            ongoing.minFreeHeap = sample.freeHeap;
            ongoing.asyncTcpStackFree = sample.asyncTcpStackFree;
            inStall = true;
            extendStall(sample);
        }

        void WebStallMonitor::extendStall(const Sample &sample)
        {
            ongoing.durationMs = sample.nowMs - ongoing.startUptimeMs;
            if (sample.tcp.total() > ongoing.worst.total())
                ongoing.worst = sample.tcp;
            ongoing.minFreeHeap = minimum(ongoing.minFreeHeap, sample.freeHeap);
            ongoing.asyncTcpStackFree = minimum(ongoing.asyncTcpStackFree, sample.asyncTcpStackFree);
            ++ongoing.samples;
            if (sample.asyncTcp == TaskActivity::Idle)
                ++ongoing.asyncTcpIdle;
            if (ongoing.durationMs >= STALL_GIVE_UP_MS)
            {
                endStall(false);
                gaveUp = true;
            }
        }

        void WebStallMonitor::endStall(bool resumed)
        {
            ongoing.resumed = resumed;
            history[head] = ongoing;
            head = (head + 1) % STALL_HISTORY;
            if (stored < STALL_HISTORY)
                ++stored;
            ++stallsTotal;
            inStall = false;
        }
    } // namespace WebHealth
} // namespace SQM
