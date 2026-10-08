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
            virtual std::string location() const = 0;     // device name
            virtual std::string timestampUtc() const = 0; // ISO 8601, or "" if the clock isn't set
        };

        struct ServerIdentity
        {
            std::string serverName;
            std::string manufacturer;
            std::string version;
            uint64_t mac = 0; // for UniqueIDs
        };

        class Router
        {
        public:
            Router(Backend &backend, ServerIdentity identity);

            // Handles /management/... and /api/v1/...; false for any other path.
            bool handle(const Request &request, Response &response);

            // A client (N.I.N.A.) currently has either device connected.
            bool anyConnected() const { return connected[0] || connected[1]; }

        private:
            Response device(const Request &request);

            Backend &backend;
            ServerIdentity identity;
            bool connected[2] = {false, false};
            uint32_t serverTransactionId = 0;
        };

    } // namespace Alpaca
} // namespace SQM
