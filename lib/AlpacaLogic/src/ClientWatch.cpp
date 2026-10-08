#include "ClientWatch.h"

namespace SQM
{
    namespace Alpaca
    {
        void ClientWatch::update(const DeviceActivity (&activity)[DEVICE_COUNT], const uint32_t (&silenceMs)[DEVICE_COUNT],
                                 bool alpacaEnabled, uint32_t nowMs)
        {
            for (size_t i = 0; i < DEVICE_COUNT; ++i)
            {
                const DeviceActivity &now = activity[i];
                Seen &last = seen[i];
                ClientState &state = states[i];
                const bool firstPass = !last.initialized;
                const bool requested = !firstPass && now.requests != last.requests;
                const bool disconnected = !firstPass && now.disconnects != last.disconnects;
                last.initialized = true;
                last.requests = now.requests;
                last.disconnects = now.disconnects;

                state.connected = now.connected;
                state.hasClientId = now.hasClientId;
                state.clientId = now.clientId;
                state.disconnectedNow = false;

                if (!alpacaEnabled)
                {
                    // Alpaca switched off: nobody is watching, nothing to report.
                    state = ClientState{};
                    last.endedCleanly = false;
                    continue;
                }

                if (requested)
                {
                    state.everRequested = true;
                    state.lastRequestMs = nowMs;
                    state.silent = false;
                }
                if (disconnected)
                {
                    state.disconnectedNow = true;
                    state.silent = false;
                    last.endedCleanly = true;
                }
                if (now.connected)
                    last.endedCleanly = false;
                // A connect counts as a request, so lastRequestMs covers "a
                // client connects but never polls".
                state.watching = now.connected || (state.everRequested && !last.endedCleanly);
                if (state.watching && !state.silent && state.everRequested && nowMs - state.lastRequestMs > silenceMs[i])
                    state.silent = true;
                if (!state.watching)
                    state.silent = false;
            }
        }

        void ClientWatch::reset()
        {
            for (size_t i = 0; i < DEVICE_COUNT; ++i)
            {
                states[i] = ClientState{};
                seen[i] = Seen{};
            }
        }

    } // namespace Alpaca
} // namespace SQM
