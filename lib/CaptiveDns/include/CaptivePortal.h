#pragma once

#include <cstdint>
#include <string>

namespace SQM
{
    // When the setup hotspot opens, and what an unknown address gets while it
    // is open (specs/014-wifi-setup). Kept free of Arduino for tests.
    namespace CaptivePortal
    {
        // With saved credentials the device keeps trying its network this long
        // before it also opens the hotspot. Long enough that a slow router or a
        // weak signal at boot doesn't put the device into setup mode (and
        // restart it once it joins); short enough that a wrong password still
        // gets the hotspot within a minute.
        constexpr uint32_t FALLBACK_AFTER_MS = 45000;

        // Whether to open the setup hotspot now: at once without saved
        // credentials, otherwise only after FALLBACK_AFTER_MS without joining.
        bool shouldOpenHotspot(bool hasCredentials, bool connected, uint32_t msTrying);

        enum class NotFound
        {
            SetupScreen, // redirect to the setup screen on the hotspot
            AlpacaError, // 400 plain text (Alpaca spec)
            ApiError,    // 404 JSON
            FileMissing, // 404: a file the app asked for (e.g. /lang.json)
            AppPage,     // index.html: a page of the app
        };

        // What a request for an unknown address gets. Only requests that came
        // in over the hotspot, for another site's name, go to the setup screen;
        // the home network never does, even while the hotspot is open.
        NotFound notFound(const std::string &path, bool viaHotspot, bool hostIsDevice);

        // A phone's "is there internet?" probe (/generate_204 and the like):
        // the setup screen over the hotspot; otherwise an ordinary unknown address.
        inline bool probeOpensSetup(bool viaHotspot)
        {
            return viaHotspot;
        }
    } // namespace CaptivePortal
} // namespace SQM
