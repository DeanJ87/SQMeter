#pragma once

#include <cstddef>
#include <cstdint>

// Web server health: lwIP TCP connection counts and the "web server stops
// answering" stalls seen since the Arduino-ESP32 3.x port (every client gets
// no answer for 15-25 s, then it recovers by itself). The firmware samples
// once a second from the loop task; this decides what is a stall and keeps
// the evidence for /api/status (connections.tcp, connections.stalls).

namespace SQM
{
    namespace WebHealth
    {
        // lwIP's enum tcp_state, in its order (static_assert'ed on the device).
        enum class TcpState : uint8_t
        {
            Closed,
            Listen,
            SynSent,
            SynRcvd,
            Established,
            FinWait1,
            FinWait2,
            CloseWait,
            Closing,
            LastAck,
            TimeWait,
        };
        constexpr size_t TCP_STATE_COUNT = 11;

        // camelCase name of a state, for JSON.
        const char *stateName(TcpState state);

        // How many TCP PCBs are in each state at one moment.
        struct TcpCounts
        {
            uint8_t byState[TCP_STATE_COUNT] = {};
            // Handshakes the listener holds that the server hasn't taken yet
            // (lwIP accepts_pending; AsyncTCP listens with a backlog of 5).
            uint8_t listenBacklog = 0;

            // One more PCB in `state` (a state number lwIP reports); saturates.
            void add(uint8_t state);
            uint8_t count(TcpState state) const { return byState[static_cast<size_t>(state)]; }
            // PCBs drawn from the connection pool (CONFIG_LWIP_MAX_ACTIVE_TCP):
            // all but listeners.
            size_t total() const;
        };

        // What async_tcp (the task that runs every request) was doing.
        enum class TaskActivity : uint8_t
        {
            Unknown, // task not found
            Idle,    // waiting for work
            Busy,    // running or waiting for the CPU
        };

        // One loop-task sample.
        struct Sample
        {
            uint32_t nowMs = 0;          // uptime
            uint32_t epoch = 0;          // 0 = clock not set
            uint32_t accepted = 0;       // web connections accepted since boot
            uint32_t alpacaRequests = 0; // Alpaca device requests since boot
            TcpCounts tcp;
            uint32_t freeHeap = 0;
            uint32_t asyncTcpStackFree = 0; // async_tcp's stack high-water mark
            TaskActivity asyncTcp = TaskActivity::Unknown;
        };

        // The most PCBs in use at once since boot.
        struct Peak
        {
            TcpCounts tcp;
            uint32_t uptimeMs = 0;
            uint32_t epoch = 0;
        };

        // A gap in accepted connections while an imaging app was polling.
        struct Stall
        {
            uint32_t startUptimeMs = 0; // the last connection accepted before the gap
            uint32_t startEpoch = 0;    // 0 = clock not set
            uint32_t durationMs = 0;    // to the first connection after (to the second)
            bool resumed = false;       // false: given up after STALL_GIVE_UP_MS
            TcpCounts atDetection;      // when the gap reached STALL_AFTER_MS
            TcpCounts worst;            // the sample with the most PCBs
            uint32_t minFreeHeap = 0;
            uint32_t asyncTcpStackFree = 0;
            uint16_t samples = 0;
            uint16_t asyncTcpIdle = 0; // samples with async_tcp waiting for work
        };

        // Decides when the web server has stalled and records the evidence.
        // Pure: time and device state come in through update().
        class WebStallMonitor
        {
        public:
            // No new connection for this long is a stall...
            static constexpr uint32_t STALL_AFTER_MS = 5000;
            // ...if an imaging app polled Alpaca this recently (N.I.N.A.
            // polls every 2 s, each request on a new connection).
            static constexpr uint32_t POLLING_WINDOW_MS = 60000;
            // A gap this long is the app going away (or a server that never
            // came back): the stall is closed unresumed.
            static constexpr uint32_t STALL_GIVE_UP_MS = 60000;
            static constexpr size_t STALL_HISTORY = 8;

            // Call about once a second.
            void update(const Sample &sample);

            const Peak &peak() const { return peakSeen; }
            // Stalls recorded since boot (the history keeps the newest).
            uint32_t stallCount() const { return stallsTotal; }
            size_t storedStalls() const { return stored; }
            // 0 = newest.
            const Stall &stall(size_t index) const;
            bool stalled() const { return inStall; }
            // The one in progress (valid while stalled()).
            const Stall &current() const { return ongoing; }

        private:
            void updatePeak(const Sample &sample);
            bool clientPolling(uint32_t nowMs) const;
            void beginStall(const Sample &sample);
            void extendStall(const Sample &sample);
            void endStall(bool resumed);

            bool started = false;
            uint32_t lastAccepted = 0;
            uint32_t lastAcceptMs = 0;
            uint32_t lastAlpacaRequests = 0;
            bool alpacaSeen = false;
            uint32_t lastAlpacaMs = 0;

            Peak peakSeen;
            bool inStall = false;
            bool gaveUp = false; // until the next connection
            Stall ongoing;
            Stall history[STALL_HISTORY];
            size_t head = 0; // next write index
            size_t stored = 0;
            uint32_t stallsTotal = 0;
        };
    } // namespace WebHealth
} // namespace SQM
