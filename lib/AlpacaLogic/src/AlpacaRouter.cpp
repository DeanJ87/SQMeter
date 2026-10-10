#include "AlpacaRouter.h"

#include "AlpacaProtocol.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <optional>

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

            struct DevicePath
            {
                std::string method;
                bool isSafetyMonitor = false;
            };

            // /api/v1/<type>/<number>/<method>, for device 0 of the two types.
            bool parseDevicePath(const std::string &path, DevicePath &out)
            {
                const size_t typeStart = 8; // strlen("/api/v1/")
                const size_t numberSlash = path.find('/', typeStart);
                const size_t methodSlash = numberSlash == std::string::npos ? std::string::npos : path.find('/', numberSlash + 1);
                if (methodSlash == std::string::npos || path.find('/', methodSlash + 1) != std::string::npos)
                    return false;
                const std::string type = path.substr(typeStart, numberSlash - typeStart);
                const std::string number = path.substr(numberSlash + 1, methodSlash - numberSlash - 1);
                out.method = path.substr(methodSlash + 1);
                out.isSafetyMonitor = type == "safetymonitor";
                return number == "0" && (out.isSafetyMonitor || type == "observingconditions") && !out.method.empty();
            }

            // The device's own web UI (the Alpaca page's live state) tags its
            // requests source=ui: it isn't an imaging app watching the device.
            void recordActivity(const Request &request, DeviceActivity &activity)
            {
                const std::string *source = findParam(request, "source");
                if (source != nullptr && *source == "ui")
                    return;
                ++activity.requests;
                if (const std::string *clientId = findParam(request, "ClientID"))
                {
                    activity.hasClientId = true;
                    activity.clientId = parseClientTransactionId(*clientId);
                }
            }

            void setConnected(DeviceActivity &activity, bool value)
            {
                if (activity.connected && !value)
                    ++activity.disconnects;
                activity.connected = value;
            }

            // One device request: what was asked, of which device, and the
            // router state it can change.
            struct DeviceCall
            {
                const Request &request;
                Backend &backend;
                const ServerIdentity &identity;
                uint32_t &serverTransactionId;
                DeviceActivity &activity;
                std::string method;
                bool get;
                bool put;
                bool isSafetyMonitor;
                bool enabled;

                Envelope reply(size_t capacity = 384) const { return Envelope(request, serverTransactionId, capacity); }
                bool is(const char *name) const { return method == name; }
            };

            // --- Common ASCOM device API ---

            std::optional<Response> commonGet(DeviceCall &call)
            {
                const bool safety = call.isSafetyMonitor;
                Envelope reply = call.reply();
                if (call.is("connected"))
                    reply.setValue(call.enabled && call.activity.connected);
                // Platform 7 asynchronous connect completes instantly, so
                // Connecting is always false.
                else if (call.is("connecting"))
                    reply.setValue(false);
                else if (call.is("name"))
                    reply.setValue(safety ? SAFETY_NAME : CONDITIONS_NAME);
                else if (call.is("description"))
                    reply.setValue(safety ? SAFETY_DESCRIPTION : CONDITIONS_DESCRIPTION);
                else if (call.is("driverinfo"))
                    reply.setValue(DRIVER_INFO);
                else if (call.is("driverversion"))
                    reply.setValue(call.identity.version);
                else if (call.is("interfaceversion"))
                    reply.setValue(safety ? SAFETY_MONITOR_INTERFACE_VERSION : OBSERVING_CONDITIONS_INTERFACE_VERSION);
                else if (call.is("supportedactions"))
                    reply.valueArray();
                else
                    return std::nullopt;
                return reply.finish();
            }

            std::optional<Response> commonPut(DeviceCall &call)
            {
                Envelope reply = call.reply();
                if (call.is("connected"))
                {
                    const std::string *param = findParam(call.request, "Connected");
                    bool value = false;
                    if (param == nullptr || !parseAlpacaBool(*param, value))
                        return badRequest("Missing or invalid Connected parameter (expected true or false)");
                    if (value && !call.enabled)
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    setConnected(call.activity, value);
                    return reply.finish();
                }
                if (call.is("connect"))
                {
                    if (!call.enabled)
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    setConnected(call.activity, true);
                    return reply.finish();
                }
                if (call.is("disconnect"))
                {
                    setConnected(call.activity, false);
                    return reply.finish();
                }
                // No custom actions or raw commands are supported.
                if (call.is("action") || call.is("commandblind") || call.is("commandbool") || call.is("commandstring"))
                    return reply.finish(ALPACA_ERR_NOT_IMPLEMENTED, "Custom actions and commands are not supported");
                return std::nullopt;
            }

            std::optional<Response> commonMethod(DeviceCall &call)
            {
                if (call.get)
                    return commonGet(call);
                if (call.put)
                    return commonPut(call);
                return std::nullopt;
            }

            void addTimestamp(JsonArray &state, const std::string &timestamp)
            {
                if (timestamp.empty())
                    return;
                JsonObject item = state.createNestedObject();
                item["Name"] = "TimeStamp";
                item["Value"] = timestamp;
            }

            // --- SafetyMonitor ---

            Response safetyMonitorMethod(DeviceCall &call)
            {
                if (!call.get)
                    return badRequest(BAD_REQUEST);
                Envelope reply = call.reply();
                if (call.is("issafe"))
                {
                    if (!call.enabled)
                    {
                        reply.setValue(false);
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    }
                    reply.setValue(call.backend.isSafe());
                    return reply.finish();
                }
                if (call.is("devicestate"))
                {
                    JsonArray state = reply.valueArray();
                    JsonObject item = state.createNestedObject();
                    item["Name"] = "IsSafe";
                    item["Value"] = call.enabled && call.backend.isSafe();
                    addTimestamp(state, call.backend.timestampUtc());
                    return reply.finish();
                }
                return badRequest(BAD_REQUEST);
            }

            // --- ObservingConditions ---

            Response averagePeriodPut(DeviceCall &call)
            {
                const std::string *param = findParam(call.request, "AveragePeriod");
                double hours = 0.0;
                if (param == nullptr || !parseAlpacaDouble(*param, hours))
                    return badRequest("Missing or invalid AveragePeriod parameter");
                const PropertyResult result = validateAveragePeriod(hours);
                return call.reply().finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
            }

            Response sensorInfo(DeviceCall &call)
            {
                const std::string *param = findParam(call.request, "SensorName");
                if (param == nullptr)
                    return badRequest("Missing SensorName parameter");
                Envelope reply = call.reply();
                if (call.is("sensordescription"))
                {
                    const StringResult result = getSensorDescription(*param, call.backend.observingConditions());
                    reply.setValue(result.value);
                    return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
                }
                const PropertyResult result = getTimeSinceLastUpdate(*param, call.backend.observingConditions());
                reply.setValue(result.ok ? result.value : 0.0);
                return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
            }

            Response observingDeviceState(DeviceCall &call)
            {
                Envelope stateReply = call.reply(1536);
                JsonArray state = stateReply.valueArray();
                if (call.enabled)
                {
                    // DeviceState lists only properties that currently have a value.
                    const ObservingConditionsSnapshot snapshot = call.backend.observingConditions();
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
                addTimestamp(state, call.backend.timestampUtc());
                return stateReply.finish();
            }

            Response observingProperty(DeviceCall &call)
            {
                for (const ObservingPropertyName &property : OBSERVING_PROPERTIES)
                {
                    if (!call.is(property.route))
                        continue;
                    Envelope reply = call.reply();
                    if (!call.enabled)
                    {
                        reply.setValue(0);
                        return reply.finish(ALPACA_ERR_NOT_CONNECTED, DISABLED_MESSAGE);
                    }
                    const PropertyResult result = getObservingConditionsProperty(property.route, call.backend.observingConditions());
                    reply.setValue(result.ok ? result.value : 0.0);
                    return reply.finish(result.ok ? 0 : result.errorNumber, result.errorMessage);
                }
                return badRequest(BAD_REQUEST);
            }

            Response observingMethod(DeviceCall &call)
            {
                if (call.put)
                {
                    if (call.is("averageperiod"))
                        return averagePeriodPut(call);
                    // Readings refresh every sensor cycle already; nothing to force.
                    if (call.is("refresh"))
                        return call.reply().finish();
                    return badRequest(BAD_REQUEST);
                }
                if (!call.get)
                    return badRequest(BAD_REQUEST);
                if (call.is("averageperiod"))
                {
                    Envelope reply = call.reply();
                    reply.setValue(0.0);
                    return reply.finish();
                }
                if (call.is("sensordescription") || call.is("timesincelastupdate"))
                    return sensorInfo(call);
                if (call.is("devicestate"))
                    return observingDeviceState(call);
                return observingProperty(call);
            }
        } // namespace

        std::string formatLocation(double latitude, double longitude)
        {
            char text[48];
            snprintf(text, sizeof(text), "%.4f, %.4f", latitude, longitude);
            return text;
        }

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
                // ServerName is this server's name - the device's (Alpaca
                // management API); Manufacturer stays the product.
                const std::string name = backend.serverName();
                value["ServerName"] = name.empty() ? identity.serverName : name;
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
            DevicePath path;
            if (!parseDevicePath(request.path, path))
                return badRequest(BAD_REQUEST);
            const size_t deviceIndex = path.isSafetyMonitor ? SAFETY_MONITOR : OBSERVING_CONDITIONS;
            DeviceCall call{
                request,
                backend,
                identity,
                serverTransactionId,
                devices[deviceIndex],
                path.method,
                request.get,
                request.put,
                path.isSafetyMonitor,
                backend.alpacaEnabled()}; // dep: D-20
            recordActivity(request, call.activity);
            if (std::optional<Response> common = commonMethod(call))
                return *common;
            return call.isSafetyMonitor ? safetyMonitorMethod(call) : observingMethod(call);
        }

    } // namespace Alpaca
} // namespace SQM
