#include "WebServer.h"
#include "ConnectionBudget.h"
#include "Logger.h"

// Live updates: the dashboard's /ws/sensors and the System page's /ws/status,
// kept within the connection budget (spec 011 FR-008).

namespace SQM
{
    void WebServer::setupWebSocket()
    {
        // Sensor WebSocket for Dashboard (/ws/sensors)
        wsSensors.onEvent(
            [this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
            { onSensorWebSocketEvent(client, type); });

        // Status WebSocket for System page (/ws/status)
        wsStatus.onEvent(
            [this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
            { onStatusWebSocketEvent(client, type); });

        server.addHandler(&wsSensors);
        server.addHandler(&wsStatus);
    }

    void WebServer::prepareWebSocketClient(std::map<uint32_t, uint32_t> &clients, AsyncWebSocketClient *client)
    {
        // Ping quiet clients so a peer that vanished is noticed (AsyncTCP drops
        // one that doesn't acknowledge); spec 011 FR-008.
        client->keepAlivePeriod(ConnectionBudget::WEBSOCKET_PING_SECONDS);
        const std::lock_guard<std::mutex> lock(wsClientsLock);
        clients[client->id()] = 0;
    }

    void WebServer::noteWebSocketClosed(std::map<uint32_t, uint32_t> &clients, AsyncWebSocketClient *client)
    {
        const std::lock_guard<std::mutex> lock(wsClientsLock);
        clients.erase(client->id());
    }

    void WebServer::capWebSocketClients(AsyncWebSocket &socket)
    {
        // cleanupClients closes the oldest client past the limit, one per pass.
        if (ConnectionBudget::overLimit(socket.count()))
            ++wsReplaced;
        socket.cleanupClients(ConnectionBudget::WEBSOCKETS_PER_ENDPOINT);
    }

    void WebServer::closeStalledClients(AsyncWebSocket &socket, std::map<uint32_t, uint32_t> &clients, uint32_t now)
    {
        // A tab in a sleeping browser keeps its socket open but stops reading:
        // its queue stays full. Close it so it doesn't hold a connection.
        // The library calls our connect handler while holding its own lock, so
        // never hold wsClientsLock while calling into the socket: copy, check,
        // write back.
        std::map<uint32_t, uint32_t> snapshot;
        {
            const std::lock_guard<std::mutex> lock(wsClientsLock);
            snapshot = clients;
        }
        for (auto &entry : snapshot)
        {
            // By id, under the socket's own lock: the network task can drop a
            // client at any moment.
            if (socket.availableForWrite(entry.first))
                entry.second = 0;
            else if (entry.second == 0)
                entry.second = now == 0 ? 1 : now;
            else if (ConnectionBudget::stalledTooLong(entry.second, now))
            {
                ++wsStalled;
                entry.second = 0;
                socket.close(entry.first);
            }
        }
        const std::lock_guard<std::mutex> lock(wsClientsLock);
        for (const auto &entry : snapshot)
        {
            auto found = clients.find(entry.first);
            if (found != clients.end())
                found->second = entry.second;
        }
    }

    void WebServer::broadcastSensorData()
    {
        // A client that stops reading is closed after a while
        // (closeStalledClients); a full queue just skips it meanwhile, so one
        // stuck tab never holds up the rest.
        closeStalledClients(wsSensors, wsSensorClients, millis());
        if (wsSensors.count() == 0)
            return;

        // Send only sensor data to Dashboard clients
        std::string json = createSensorDataJson();
        wsSensors.textAll(json.c_str());
    }

    void WebServer::broadcastStatusData()
    {
        closeStalledClients(wsStatus, wsStatusClients, millis());
        if (wsStatus.count() == 0)
            return;

        // Send only status data to System page clients
        std::string json = createStatusJson();
        wsStatus.textAll(json.c_str());
    }

    void WebServer::onSensorWebSocketEvent(AsyncWebSocketClient *client, AwsEventType type)
    {
        switch (type)
        {
        case WS_EVT_CONNECT:
            prepareWebSocketClient(wsSensorClients, client);
            Logger::info(TAG, "Sensor WebSocket client connected: %u", client->id());
            // Send initial sensor data
            client->text(createSensorDataJson().c_str());
            break;

        case WS_EVT_DISCONNECT:
            noteWebSocketClosed(wsSensorClients, client);
            Logger::info(TAG, "Sensor WebSocket client disconnected: %u", client->id());
            break;

        default:
            break;
        }
    }

    void WebServer::onStatusWebSocketEvent(AsyncWebSocketClient *client, AwsEventType type)
    {
        switch (type)
        {
        case WS_EVT_CONNECT:
            prepareWebSocketClient(wsStatusClients, client);
            Logger::info(TAG, "Status WebSocket client connected: %u", client->id());
            // Send initial status data
            client->text(createStatusJson().c_str());
            break;

        case WS_EVT_DISCONNECT:
            noteWebSocketClosed(wsStatusClients, client);
            Logger::info(TAG, "Status WebSocket client disconnected: %u", client->id());
            break;

        case WS_EVT_ERROR:
            Logger::error(TAG, "WebSocket error: %u", client->id());
            break;

        case WS_EVT_DATA:
            // Handle incoming WebSocket messages if needed
            break;

        default:
            break;
        }
    }

} // namespace SQM
