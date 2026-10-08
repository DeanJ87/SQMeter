#include "AlpacaRouter.h"

#include "AlpacaProtocol.h"

#include <ArduinoJson.h>

namespace SQM
{
    namespace Alpaca
    {
        namespace
        {
            constexpr size_t SAFETY_MONITOR = 0;
            constexpr size_t OBSERVING_CONDITIONS = 1;

            // ASCOM Platform 7 versions, which add Connect/Disconnect/
            // Connecting/DeviceState to every device.
            constexpr int SAFETY_MONITOR_INTERFACE_VERSION = 3;
            constexpr int OBSERVING_CONDITIONS_INTERFACE_VERSION = 2;

            const char *const SAFETY_NAME = "SQMeter SafetyMonitor";
            const char *const SAFETY_DESCRIPTION = "Reports observatory safety from rain (RG-15), wind (optional anemometer), cloud cover, "
                                                   "sky brightness, humidity and dew-point margin measured by the onboard SQMeter sensors.";
            const char *const CONDITIONS_NAME = "SQMeter ObservingConditions";
            const char *const CONDITIONS_DESCRIPTION =
                "Reports sky quality, sky brightness, cloud cover, sky temperature, temperature, humidity, dew point, pressure, and - when "
                "fitted - rain rate (RG-15) and wind speed, gust and direction (anemometer/vane).";
            const char *const DRIVER_INFO = "Native ESP32 firmware, no external bridge - https://github.com/DeanJ87/SQMeter";
            const char *const DISABLED_MESSAGE = "Alpaca support is disabled in device settings";
            const char *const BAD_REQUEST = "Invalid Alpaca device type, device number, method or HTTP verb";

            struct ObservingPropertyName
            {
                const char *route;     // lowercase Alpaca method name
                const char *stateName; // PascalCase name used in DeviceState
            };

            constexpr ObservingPropertyName OBSERVING_PROPERTIES[] = {
                {"cloudcover", "CloudCover"},
                {"dewpoint", "DewPoint"},
                {"humidity", "Humidity"},
                {"pressure", "Pressure"},
                {"rainrate", "RainRate"},
                {"skybrightness", "SkyBrightness"},
                {"skyquality", "SkyQuality"},
                {"skytemperature", "SkyTemperature"},
                {"starfwhm", "StarFWHM"},
                {"temperature", "Temperature"},
                {"winddirection", "WindDirection"},
                {"windgust", "WindGust"},
                {"windspeed", "WindSpeed"}};

            // Alpaca parameter names are case-insensitive in a GET query
            // string, case-sensitive in a PUT form body.
            const std::string *findParam(const Request &request, const char *name)
            {
                for (const auto &param : request.params)
                    if (paramNameMatches(param.first, name, request.put))
                        return &param.second;
                return nullptr;
            }

            Response badRequest(const char *message)
            {
                Response response;
                response.status = 400;
                response.contentType = "text/plain";
                response.body = message;
                return response;
            }

            // Every Alpaca reply carries the same transaction/error fields.
            class Envelope
            {
            public:
                Envelope(const Request &request, uint32_t &serverTransactionId, size_t capacity = 384)
                    : doc(capacity),
                      request(request),
                      serverTransactionId(serverTransactionId)
                {
                }

                template <typename T> void setValue(const T &value) { doc["Value"] = value; }
                JsonArray valueArray() { return doc.createNestedArray("Value"); }
                JsonObject valueObject() { return doc.createNestedObject("Value"); }

                Response finish(int errorNumber = 0, const std::string &errorMessage = "")
                {
                    const std::string *txn = findParam(request, "ClientTransactionID");
                    doc["ClientTransactionID"] = txn != nullptr ? parseClientTransactionId(*txn) : 0;
                    doc["ServerTransactionID"] = ++serverTransactionId;
                    doc["ErrorNumber"] = errorNumber;
                    doc["ErrorMessage"] = errorMessage;
                    Response response;
                    serializeJson(doc, response.body);
                    return response;
                }

            private:
                DynamicJsonDocument doc;
                const Request &request;
                uint32_t &serverTransactionId;
            };
        } // namespace

        Router::Router(Backend &backend, ServerIdentity identity)
            : backend(backend),
              identity(std::move(identity))
        {
        }

        void Router::resetConnections()
        {
            for (DeviceActivity &activity : devices)
                activity.connected = false;
        }

        bool Router::handle(const Request &request, Response &response)
        {
            const std::string &path = request.path;
            if (path == "/management/apiversions" && request.get)
            {
                Envelope reply(request, serverTransactionId);
                reply.valueArray().add(1);
                response = reply.finish();
                return true;
            }
            if (path == "/management/v1/description" && request.get)
            {
                Envelope reply(request, serverTransactionId);
                JsonObject value = reply.valueObject();
                value["ServerName"] = identity.serverName;
                value["Manufacturer"] = identity.manufacturer;
                value["ManufacturerVersion"] = identity.version;
                value["Location"] = backend.location();
                response = reply.finish();
                return true;
            }
            if (path == "/management/v1/configureddevices" && request.get)
            {
                Envelope reply(request, serverTransactionId, 768);
                JsonArray value = reply.valueArray();
                if (backend.alpacaEnabled())
                {
                    JsonObject safety = value.createNestedObject();
                    safety["DeviceName"] = SAFETY_NAME;
                    safety["DeviceType"] = "SafetyMonitor";
                    safety["DeviceNumber"] = 0;
                    safety["UniqueID"] = buildUniqueId(identity.mac, "safetymonitor", 0);

                    JsonObject conditions = value.createNestedObject();
                    conditions["DeviceName"] = CONDITIONS_NAME;
                    conditions["DeviceType"] = "ObservingConditions";
                    conditions["DeviceNumber"] = 0;
                    conditions["UniqueID"] = buildUniqueId(identity.mac, "observingconditions", 0);
                }
                response = reply.finish();
                return true;
            }
            if (path.rfind("/api/v1/", 0) == 0)
            {
                response = device(request);
                return true;
            }
            if (path.rfind("/management/", 0) == 0)
            {
                response = badRequest(BAD_REQUEST);
                return true;
            }
            return false;
        }

        Response Router::device(const Request &request)
        {
            // /api/v1/<type>/<number>/<method>
            const std::string &path = request.path;
            const size_t typeStart = 8; // strlen("/api/v1/")
            const size_t numberSlash = path.find('/', typeStart);
            const size_t methodSlash = numberSlash == std::string::npos ? std::string::npos : path.find('/', numberSlash + 1);
            if (methodSlash == std::string::npos || path.find('/', methodSlash + 1) != std::string::npos)
                return badRequest(BAD_REQUEST);
            const std::string type = path.substr(typeStart, numberSlash - typeStart);
            const std::string number = path.substr(numberSlash + 1, methodSlash - numberSlash - 1);
            const std::string method = path.substr(methodSlash + 1);
            const bool isSafetyMonitor = type == "safetymonitor";
            if (number != "0" || (!isSafetyMonitor && type != "observingconditions") || method.empty())
                return badRequest(BAD_REQUEST);

            const bool get = request.get;
            const bool put = request.put;
            const size_t deviceIndex = isSafetyMonitor ? SAFETY_MONITOR : OBSERVING_CONDITIONS;
            const bool enabled = backend.alpacaEnabled();
            DeviceActivity &activity = devices[deviceIndex];
            // The device's own web UI (the Alpaca page's live state) tags its
            // requests source=ui: it isn't an imaging app watching the device.
            const std::string *source = findParam(request, "source");
            if (source == nullptr || *source != "ui")
            {
                ++activity.requests;
                if (const std::string *clientId = findParam(request, "ClientID"))
                {
                    activity.hasClientId = true;
                    activity.clientId = parseClientTransactionId(*clientId);
                }
            }
            auto setConnected = [&activity](bool value)
            {
                if (activity.connected && !value)
                    ++activity.disconnects;
                activity.connected = value;
            };
            Envelope reply(request, serverTransactionId);

            // --- Common ASCOM device API ---
            if (method == "connected" && get)
            {
                reply.setValue(enabled && activity.connected);
                return reply.finish();
            }
            if (method == "connected" && put)
            {
                const std::string *param = findParam(request, "Connected");
                bool value = false;
                if (param == nullptr || !parseAlpacaBool(*param, value))
                    return badRequest("Missing or invalid Connected parameter (expected true or false)");
                if (value && !enabled)
                    return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                setConnected(value);
                return reply.finish();
            }
            // Platform 7 asynchronous connect: connecting completes instantly,
            // so Connecting is always false.
            if (method == "connect" && put)
            {
                if (!enabled)
                    return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                setConnected(true);
                return reply.finish();
            }
            if (method == "disconnect" && put)
            {
                setConnected(false);
                return reply.finish();
            }
            if (method == "connecting" && get)
            {
                reply.setValue(false);
                return reply.finish();
            }
            if (method == "name" && get)
            {
                reply.setValue(isSafetyMonitor ? SAFETY_NAME : CONDITIONS_NAME);
                return reply.finish();
            }
            if (method == "description" && get)
            {
                reply.setValue(isSafetyMonitor ? SAFETY_DESCRIPTION : CONDITIONS_DESCRIPTION);
                return reply.finish();
            }
            if (method == "driverinfo" && get)
            {
                reply.setValue(DRIVER_INFO);
                return reply.finish();
            }
            if (method == "driverversion" && get)
            {
                reply.setValue(identity.version);
                return reply.finish();
            }
            if (method == "interfaceversion" && get)
            {
                reply.setValue(isSafetyMonitor ? SAFETY_MONITOR_INTERFACE_VERSION : OBSERVING_CONDITIONS_INTERFACE_VERSION);
                return reply.finish();
            }
            if (method == "supportedactions" && get)
            {
                reply.valueArray();
                return reply.finish();
            }
            // No custom actions or raw commands are supported.
            if (put && (method == "action" || method == "commandblind" || method == "commandbool" || method == "commandstring"))
                return reply.finish(ALPACA_ERR_NOT_IMPLEMENTED, "Custom actions and commands are not supported");

            const std::string timestamp = backend.timestampUtc();
            auto addTimestamp = [&timestamp](JsonArray &state)
            {
                if (timestamp.empty())
                    return;
                JsonObject item = state.createNestedObject();
                item["Name"] = "TimeStamp";
                item["Value"] = timestamp;
            };

            // --- SafetyMonitor ---
            if (isSafetyMonitor)
            {
                if (method == "issafe" && get)
                {
                    if (!enabled)
                    {
                        reply.setValue(false);
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    }
                    reply.setValue(backend.isSafe());
                    return reply.finish();
                }
                if (method == "devicestate" && get)
                {
                    JsonArray state = reply.valueArray();
                    JsonObject item = state.createNestedObject();
                    item["Name"] = "IsSafe";
                    item["Value"] = enabled && backend.isSafe();
                    addTimestamp(state);
                    return reply.finish();
                }
                return badRequest(BAD_REQUEST);
            }

            // --- ObservingConditions ---
            if (method == "averageperiod" && get)
            {
                reply.setValue(0.0);
                return reply.finish();
            }
            if (method == "averageperiod" && put)
            {
                const std::string *param = findParam(request, "AveragePeriod");
                double hours = 0.0;
                if (param == nullptr || !parseAlpacaDouble(*param, hours))
                    return badRequest("Missing or invalid AveragePeriod parameter");
                const PropertyResult result = validateAveragePeriod(hours);
                return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
            }
            // Readings refresh every sensor cycle already; nothing to force.
            if (method == "refresh" && put)
                return reply.finish();
            if ((method == "sensordescription" || method == "timesincelastupdate") && get)
            {
                const std::string *param = findParam(request, "SensorName");
                if (param == nullptr)
                    return badRequest("Missing SensorName parameter");
                if (method == "sensordescription")
                {
                    const StringResult result = getSensorDescription(*param, backend.observingConditions());
                    reply.setValue(result.value);
                    return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
                }
                const PropertyResult result = getTimeSinceLastUpdate(*param, backend.observingConditions());
                reply.setValue(result.ok ? result.value : 0.0);
                return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
            }
            if (method == "devicestate" && get)
            {
                Envelope stateReply(request, serverTransactionId, 1536);
                JsonArray state = stateReply.valueArray();
                if (enabled)
                {
                    // DeviceState lists only properties that currently have a value.
                    const ObservingConditionsSnapshot snapshot = backend.observingConditions();
                    for (const ObservingPropertyName &property : OBSERVING_PROPERTIES)
                    {
                        const PropertyResult result = getObservingConditionsProperty(property.route, snapshot);
                        if (!result.ok)
                            continue;
                        JsonObject item = state.createNestedObject();
                        item["Name"] = property.stateName;
                        item["Value"] = result.value;
                    }
                }
                addTimestamp(state);
                return stateReply.finish();
            }
            if (get)
            {
                for (const ObservingPropertyName &property : OBSERVING_PROPERTIES)
                {
                    if (method != property.route)
                        continue;
                    if (!enabled)
                    {
                        reply.setValue(0);
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    }
                    const PropertyResult result = getObservingConditionsProperty(property.route, backend.observingConditions());
                    reply.setValue(result.ok ? result.value : 0.0);
                    return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
                }
            }
            return badRequest(BAD_REQUEST);
        }

    } // namespace Alpaca
} // namespace SQM
