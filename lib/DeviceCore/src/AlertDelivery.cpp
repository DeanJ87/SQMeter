#include "AlertDelivery.h"

#include <ArduinoJson.h>

namespace SQM
{
    namespace Delivery
    {
        namespace
        {
            // Emergency: Pushover repeats every minute for up to an hour until acknowledged.
            constexpr const char *PUSHOVER_EMERGENCY = "&retry=60&expire=3600";
            constexpr int PUSHOVER_EMERGENCY_PRIORITY = 2;
            constexpr size_t ALERT_JSON_CAPACITY = 768;
        } // namespace

        std::string urlEncode(const std::string &value)
        {
            static const char *hex = "0123456789ABCDEF";
            std::string out;
            out.reserve(value.size() * 3);
            for (unsigned char c : value)
            {
                const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                                        c == '_' || c == '.' || c == '~';
                if (unreserved)
                {
                    out += static_cast<char>(c);
                    continue;
                }
                out += '%';
                out += hex[c >> 4];
                out += hex[c & 0x0F];
            }
            return out;
        }

        std::string fullTitle(const std::string &deviceName, const std::string &title)
        {
            return deviceName.empty() ? title : deviceName + ": " + title;
        }

        const char *ntfyTags(Alerts::AlertType type)
        {
            // In AlertType order (lib/AlertLogic/include/AlertEngine.h).
            static constexpr const char *TAGS[] = {
                "warning",                // Unsafe
                "white_check_mark",       // Safe
                "cloud_with_rain",        // RainStarted
                "sun_behind_small_cloud", // RainStopped
                "rotating_light",         // SensorFault
                "wrench",                 // SensorRecovered
                "rotating_light",         // LensFault
                "droplet",                // DewRisk
                "star",                   // ClearSky
                "cloud",                  // CloudedOver
                "ok_hand",                // Acknowledged
                "telescope",              // AlertsOn
                "test_tube",              // Test
                "satellite",              // ClientLost
                "link",                   // ClientBack
                "electric_plug",          // ClientDisconnected
            };
            static_assert(sizeof(TAGS) / sizeof(TAGS[0]) == Alerts::ALERT_TYPE_COUNT, "a tag for every AlertType");
            const size_t index = static_cast<size_t>(type);
            return index < Alerts::ALERT_TYPE_COUNT ? TAGS[index] : "bell";
        }

        HttpRequest pushoverRequest(const Alerts::Alert &alert, const PushoverCredentials &creds, const std::string &deviceName)
        {
            const int priority = Alerts::pushoverPriority(alert.level);
            HttpRequest request;
            request.url = PUSHOVER_URL;
            request.contentType = "application/x-www-form-urlencoded";
            request.body = "token=" + urlEncode(creds.appToken) + "&user=" + urlEncode(creds.userKey) +
                           "&title=" + urlEncode(fullTitle(deviceName, alert.title)) + "&message=" + urlEncode(alert.message) +
                           "&priority=" + std::to_string(priority);
            if (priority == PUSHOVER_EMERGENCY_PRIORITY)
                request.body += PUSHOVER_EMERGENCY;
            const std::string &sound = alert.sound.empty() ? creds.defaultSound : alert.sound;
            if (!sound.empty())
                request.body += "&sound=" + urlEncode(sound);
            return request;
        }

        HttpRequest ntfyRequest(const Alerts::Alert &alert, const NtfyTarget &target, const std::string &deviceName)
        {
            std::string server = target.server;
            while (!server.empty() && server.back() == '/')
                server.pop_back();

            HttpRequest request;
            request.url = server + "/" + urlEncode(target.topic);
            request.contentType = "text/plain; charset=utf-8";
            request.headers = {
                {"Title", fullTitle(deviceName, alert.title)},
                {"Priority", Alerts::ntfyPriority(alert.level)},
                {"Tags", ntfyTags(alert.type)},
            };
            if (!target.token.empty())
                request.headers.push_back({"Authorization", "Bearer " + target.token});
            request.body = alert.message;
            return request;
        }

        std::string alertJson(const Alerts::Alert &alert, const std::string &deviceName, int64_t epochSeconds)
        {
            DynamicJsonDocument doc(ALERT_JSON_CAPACITY);
            doc["event"] = Alerts::alertTypeName(alert.type);
            if (!alert.stacked.empty())
            {
                // Several events sent as one notification: list them all, lead first.
                JsonArray events = doc.createNestedArray("events");
                events.add(Alerts::alertTypeName(alert.type));
                for (Alerts::AlertType type : alert.stacked)
                    events.add(Alerts::alertTypeName(type));
            }
            doc["title"] = alert.title;
            doc["message"] = alert.message;
            doc["level"] = Alerts::alertLevelName(alert.level);
            doc["device"] = deviceName;
            if (epochSeconds != 0)
                doc["timestamp"] = epochSeconds;
            std::string out;
            serializeJson(doc, out);
            return out;
        }
    } // namespace Delivery
} // namespace SQM
