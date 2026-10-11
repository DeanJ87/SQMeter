#pragma once

#include "ObservingConditionsMapper.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SQM
{
    namespace Alpaca
    {
        // The Alpaca HTTP API (management + SafetyMonitor/ObservingConditions
        // device routes) independent of any web server, so the firmware and
        // the native simulator that ConformU tests in CI run the same code.

        struct Request
        {
            bool get = false;
            bool put = false;
            std::string path; // e.g. /api/v1/safetymonitor/0/issafe
            // Query string (GET) or form body (PUT) parameters, URL-decoded.
            std::vector<std::pair<std::string, std::string>> params;
        };

        struct Response
        {
            int status = 200;
            const char *contentType = "application/json";
            std::string body;
        };

        // What the router needs from the device.
        class Backend
        {
        public:
            virtual ~Backend() = default;
            virtual bool alpacaEnabled() const = 0;
            virtual bool isSafe() const = 0;
            virtual ObservingConditionsSnapshot observingConditions() const = 0;
            virtual std::string serverName() const = 0;   // the device's name (Settings → Device → Name)
            virtual std::string location() const = 0;     // "lat, lon" of the location in use, or "" (formatLocation)
            virtual std::string timestampUtc() const = 0; // ISO 8601, or "" if the clock isn't set
        };

        struct ServerIdentity
        {
            std::string serverName;
            std::string manufacturer;
            std::string version;
            uint64_t mac = 0; // for UniqueIDs
        };

        // The management description's Location: where the device is, as
        // "lat, lon" in machine format ('.' decimals, 4 places).
        std::string formatLocation(double latitude, double longitude);

        // The two Alpaca devices, in Router order.
        enum class Device : uint8_t
        {
            SafetyMonitor = 0,
            ObservingConditions = 1,
        };
        constexpr size_t DEVICE_COUNT = 2;

        // What clients did with one device since the Router started. The
        // counters only ever grow; ClientWatch compares them between passes,
        // so the Router (which runs on the web server's task) needs no clock.
        struct DeviceActivity
        {
            bool connected = false;
            uint32_t requests = 0;    // every device request, any method (except the web UI's, source=ui)
            uint32_t disconnects = 0; // clean disconnects of a connected device
            bool hasClientId = false;
            uint32_t clientId = 0; // ClientID of the latest request that sent one
        };

        class Router
        {
        public:
            Router(Backend &backend, ServerIdentity identity);

            // Handles /management/... and /api/v1/...; false for any other path.
            bool handle(const Request &request, Response &response);

            // A client (N.I.N.A.) currently has either device connected.
            bool anyConnected() const { return devices[0].connected || devices[1].connected; }

            DeviceActivity activity(Device device) const { return devices[static_cast<size_t>(device)]; }

            // As after a restart: no device connected (the demo's restart).
            void resetConnections();

            // After a restart the device caused itself (ConnectionMemory): the
            // imaging app's connections as they were, without counting a
            // disconnect.
            void restoreConnections(const bool (&connected)[DEVICE_COUNT]);

            // Which devices a client has connected, in Device order.
            void connectedDevices(bool (&connected)[DEVICE_COUNT]) const;

        private:
            Response device(const Request &request);

            Backend &backend;
            ServerIdentity identity;
            DeviceActivity devices[DEVICE_COUNT];
            uint32_t serverTransactionId = 0;
        };

    } // namespace Alpaca
} // namespace SQM
