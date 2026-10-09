#pragma once

// What an alert looks like on each service: the Pushover form, the ntfy
// request and the JSON published over MQTT and posted to webhooks. The
// firmware's AlertDispatcher sends these; the browser demo sends the same
// requests when a visitor opts in (specs/018).

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "AlertEngine.h"

namespace SQM
{
    namespace Delivery
    {
        using Headers = std::vector<std::pair<std::string, std::string>>;

        struct HttpRequest
        {
            std::string url;
            std::string contentType;
            Headers headers;
            std::string body;
        };

        struct PushoverCredentials
        {
            std::string userKey;
            std::string appToken;
            std::string defaultSound; // used when the alert has none
        };

        struct NtfyTarget
        {
            std::string server; // e.g. "https://ntfy.sh"; trailing slashes ignored
            std::string topic;
            std::string token; // optional bearer token
        };

        constexpr const char *PUSHOVER_URL = "https://api.pushover.net/1/messages.json";

        std::string urlEncode(const std::string &value);
        // "Observatory: Rain detected" (just the title when there's no device name).
        std::string fullTitle(const std::string &deviceName, const std::string &title);
        const char *ntfyTags(Alerts::AlertType type);

        HttpRequest pushoverRequest(const Alerts::Alert &alert, const PushoverCredentials &creds, const std::string &deviceName);
        HttpRequest ntfyRequest(const Alerts::Alert &alert, const NtfyTarget &target, const std::string &deviceName);
        // MQTT `<base>/alerts` payload and webhook body: {event, events?, title,
        // message, level, device, timestamp?}. `epochSeconds` 0 = clock not set.
        std::string alertJson(const Alerts::Alert &alert, const std::string &deviceName, int64_t epochSeconds);
    } // namespace Delivery
} // namespace SQM
