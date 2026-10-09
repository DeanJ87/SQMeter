#include "CaptivePortal.h"

namespace SQM
{
    namespace CaptivePortal
    {
        namespace
        {
            bool startsWith(const std::string &text, const char *prefix)
            {
                return text.rfind(prefix, 0) == 0;
            }

            // "/lang.json", "/assets/x.js": the last segment has an extension.
            bool looksLikeFile(const std::string &path)
            {
                const size_t slash = path.find_last_of('/');
                const std::string last = slash == std::string::npos ? path : path.substr(slash + 1);
                return last.find('.') != std::string::npos;
            }
        } // namespace

        bool shouldOpenHotspot(bool hasCredentials, bool connected, uint32_t msTrying)
        {
            if (!hasCredentials)
                return true;
            return !connected && msTrying >= FALLBACK_AFTER_MS;
        }

        NotFound notFound(const std::string &path, bool viaHotspot, bool hostIsDevice)
        {
            if (startsWith(path, "/api/v1/"))
                return NotFound::AlpacaError;
            if (startsWith(path, "/api/"))
                return NotFound::ApiError;
            if (viaHotspot && !hostIsDevice)
                return NotFound::SetupScreen;
            return looksLikeFile(path) ? NotFound::FileMissing : NotFound::AppPage;
        }
    } // namespace CaptivePortal
} // namespace SQM
