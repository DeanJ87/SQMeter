#pragma once

#include <cstddef>

// How the device's TCP connections are shared (spec 011 FR-008, spec 013
// FR-012). lwIP allows CONFIG_LWIP_MAX_ACTIVE_TCP (16) at once; every HTTP
// request uses a fresh one (the server sends Connection: close), so an imaging
// app polling the device needs free ones all the time. Live-update sockets are
// capped so browsers can never take them all.

namespace SQM
{
    namespace ConnectionBudget
    {
        constexpr size_t TCP_CONNECTIONS = 16; // CONFIG_LWIP_MAX_ACTIVE_TCP

        // WebSocket clients per endpoint (/ws/sensors, /ws/status). A new one
        // past the limit replaces the oldest.
        constexpr size_t WEBSOCKETS_PER_ENDPOINT = 3;
        constexpr size_t WEBSOCKET_ENDPOINTS = 2;

        // Outbound: MQTT, plus one TLS connection at a time (alerts, update
        // check, language download share one lock).
        constexpr size_t OUTBOUND = 2;

        // What's always left for HTTP: N.I.N.A., ConformU, the web UI's pages.
        constexpr size_t httpReserve()
        {
            return TCP_CONNECTIONS - WEBSOCKETS_PER_ENDPOINT * WEBSOCKET_ENDPOINTS - OUTBOUND;
        }

        // An imaging app polling two devices, plus a browser loading a page.
        constexpr size_t MIN_HTTP_RESERVE = 6;

        static_assert(httpReserve() >= MIN_HTTP_RESERVE, "live-update sockets would crowd out imaging apps");

        // A live-update client is sent a ping this often; one that stops
        // acknowledging is dropped (AsyncTCP's 5 s acknowledgement timeout).
        constexpr unsigned WEBSOCKET_PING_SECONDS = 15;

        // A client whose send queue has stayed full this long has stopped
        // reading (a tab in a sleeping browser) and is closed; a brief
        // backlog on weak WiFi is not.
        constexpr unsigned long STALL_CLOSE_MS = 10000;

        // fullSinceMs: when its queue was first seen full (0 = not full).
        constexpr bool stalledTooLong(unsigned long fullSinceMs, unsigned long nowMs)
        {
            return fullSinceMs != 0 && nowMs - fullSinceMs >= STALL_CLOSE_MS;
        }

        // Should a new client's endpoint drop its oldest client?
        constexpr bool overLimit(size_t clientsOnEndpoint)
        {
            return clientsOnEndpoint > WEBSOCKETS_PER_ENDPOINT;
        }
    } // namespace ConnectionBudget
} // namespace SQM
