#pragma once

#include "AlpacaRouter.h"

#include <cstdint>

namespace SQM
{
    namespace Alpaca
    {
        // Whether an imaging app is still checking each Alpaca device. The
        // Router counts requests; this turns the counts into "watching" and
        // "silent" on the device's uptime clock (specs/021, FR-007/FR-008).
        struct ClientState
        {
            bool connected = false;
            // Connected, or polled since the restart (some clients keep
            // polling across a device restart without connecting again).
            // After a clean disconnect only a new connect counts.
            bool watching = false;
            bool silent = false;        // watching, but no request for the silence time
            bool everRequested = false; // since the restart
            uint32_t lastRequestMs = 0;
            bool hasClientId = false;
            uint32_t clientId = 0;
            bool disconnectedNow = false; // a clean disconnect since the last update
        };

        class ClientWatch
        {
        public:
            // Call regularly (every alert pass). `silenceMs` per device, in
            // Device order. With Alpaca off nothing is watched.
            void update(
                const DeviceActivity (&activity)[DEVICE_COUNT],
                const uint32_t (&silenceMs)[DEVICE_COUNT],
                bool alpacaEnabled,
                uint32_t nowMs);

            const ClientState &state(Device device) const { return states[static_cast<size_t>(device)]; }

            // As after a restart (the demo's restart).
            void reset();

        private:
            struct Seen
            {
                bool initialized = false;
                uint32_t requests = 0;
                uint32_t disconnects = 0;
                bool endedCleanly = false; // a clean disconnect since the restart
            };
            ClientState states[DEVICE_COUNT];
            Seen seen[DEVICE_COUNT];
        };

    } // namespace Alpaca
} // namespace SQM
