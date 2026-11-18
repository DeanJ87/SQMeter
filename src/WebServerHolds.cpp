#include "WebServer.h"
#include "Logger.h"
#include "SafetyHistory.h"
#include <Preferences.h>
#include <cstring>
#include <ctime>

// Safety holds (specs/035-safety-holds): the HTTP route, the MQTT command,
// NVS storage and the manual hold that follows Force unsafe. What a request
// means and does is lib/DeviceCore (DeviceHolds.cpp), shared with the demo.

namespace SQM
{
    namespace
    {
        constexpr const char *HOLDS_NVS_NAMESPACE = "sqm-holds";
        constexpr const char *HOLDS_NVS_KEY = "list";
        constexpr size_t MAX_HOLD_BODY_BYTES = 1024;

        // RAII lock: the hold list is only ever touched under it, briefly.
        class HoldsLock
        {
        public:
            explicit HoldsLock(SemaphoreHandle_t mutex)
                : mutex(mutex)
            {
                xSemaphoreTake(mutex, portMAX_DELAY);
            }
            ~HoldsLock() { xSemaphoreGive(mutex); }
            HoldsLock(const HoldsLock &) = delete;
            HoldsLock &operator=(const HoldsLock &) = delete;

        private:
            SemaphoreHandle_t mutex;
        };

        int64_t wallClock()
        {
            return static_cast<int64_t>(time(nullptr));
        }

        void collectHoldBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (total > MAX_HOLD_BODY_BYTES)
                return;
            if (index == 0 && request->_tempObject == nullptr)
                request->_tempObject = calloc(total + 1, 1);
            if (request->_tempObject != nullptr && index + len <= total)
                std::memcpy(static_cast<uint8_t *>(request->_tempObject) + index, data, len);
        }

        std::string takeHoldBody(AsyncWebServerRequest *request)
        {
            std::string body;
            if (request->_tempObject != nullptr)
            {
                body.assign(static_cast<const char *>(request->_tempObject), request->contentLength());
                free(request->_tempObject);
                request->_tempObject = nullptr;
            }
            return body;
        }
    } // namespace

    void WebServer::restoreHolds()
    {
        std::string stored;
        Preferences prefs;
        if (prefs.begin(HOLDS_NVS_NAMESPACE, true))
        {
            stored = prefs.getString(HOLDS_NVS_KEY, "").c_str();
            prefs.end();
        }
        bool changed = false;
        {
            HoldsLock lock(holdsMutex);
            if (!stored.empty() && !holds.restore(stored, millis()))
                Logger::warn(TAG, "Stored safety holds unreadable, starting with none");
            // The setting decides whether the manual hold exists (FR-008), so
            // settings from before holds keep Force unsafe as it was.
            changed = Core::syncManualHold(holds, getConfigCallback().alpaca.manualOverrideUnsafe, millis(), wallClock());
            if (changed)
                stored = holds.serialize();
            Logger::info(TAG, "Safety holds: %u", static_cast<unsigned>(holds.all().size()));
        }
        if (changed)
            storeHolds(stored);
    }

    void WebServer::storeHolds(const std::string &stored)
    {
        Preferences prefs;
        if (!prefs.begin(HOLDS_NVS_NAMESPACE, false))
        {
            Logger::error(TAG, "Couldn't open NVS to store the safety holds");
            return;
        }
        if (prefs.putString(HOLDS_NVS_KEY, stored.c_str()) == 0)
            Logger::error(TAG, "Couldn't store the safety holds (%u bytes)", static_cast<unsigned>(stored.size()));
        prefs.end();
    }

    void WebServer::setupHoldRoutes()
    {
        // Also matches /api/safety/hold/<id> and /api/safety/hold/<id>/clear.
        server.on(
            "/api/safety/hold",
            HTTP_POST | HTTP_DELETE,
            [this](AsyncWebServerRequest *request) { handleHold(request); },
            nullptr,
            collectHoldBody);
    }

    void WebServer::subscribeHoldCommands()
    {
        if (mqttClient == nullptr)
            return;
        mqttClient->onCommand(
            "safety/hold/set",
            [this](const std::string &payload)
            {
                const Core::HoldOutcome outcome = runHoldCommand(Core::holdCommandFromMqtt(payload));
                if (outcome.status != 200)
                    Logger::warn(TAG, "MQTT safety hold ignored: %s", outcome.body.c_str());
            });
    }

    void WebServer::handleHold(AsyncWebServerRequest *request)
    {
        const std::string body = takeHoldBody(request);
        if (!requireAuth(request))
            return;
        if (request->contentLength() > MAX_HOLD_BODY_BYTES)
        {
            request->send(413, "application/json", createErrorJson("Request too large").c_str());
            return;
        }
        const std::string source = request->hasParam("source") ? request->getParam("source")->value().c_str() : "";
        Core::HoldCommand command;
        if (!Core::holdCommandFromHttp(request->method() == HTTP_DELETE, request->url().c_str(), body, source, command))
        {
            request->send(404, "application/json", createErrorJson("Not found").c_str());
            return;
        }
        const Core::HoldOutcome outcome = runHoldCommand(command);
        request->send(outcome.status, "application/json", outcome.body.c_str());
    }

    Core::HoldOutcome WebServer::runHoldCommand(const Core::HoldCommand &command)
    {
        const SafetyStatus status = getSafetyStatus();
        Core::HoldOutcome outcome;
        std::string stored;
        bool manualOn = false;
        {
            HoldsLock lock(holdsMutex);
            outcome = Core::applyHold(holds, command, millis(), wallClock());
            if (!outcome.changed)
                return outcome;
            recordHoldEvent(command, outcome, status);
            stored = holds.serialize();
            manualOn = holds.find(Alpaca::MANUAL_HOLD_ID) != nullptr;
        }
        storeHolds(stored);
        Logger::info(
            TAG, "Safety hold %s %s", command.request.id.c_str(), command.kind == Core::HoldCommand::Kind::Clear ? "cleared" : "set");
        if (outcome.manualChanged)
            applyManualOverride(manualOn);
        return outcome;
    }

    // Under holdsMutex.
    void WebServer::recordHoldEvent(const Core::HoldCommand &command, const Core::HoldOutcome &outcome, const SafetyStatus &status)
    {
        SafetyHistory::Entry entry{};
        if (outcome.cleared)
        {
            Core::holdClearedEntry(entry, outcome.removed, status, holds);
            SafetyHistory::recordHold(entry);
        }
        else if (outcome.added && Core::holdSetEntry(entry, command.request.id, status, holds))
        {
            SafetyHistory::recordHold(entry);
        }
    }

    void WebServer::applyManualOverride(bool forceUnsafe)
    {
        if (manualOverrideCallback && manualOverrideCallback(forceUnsafe))
            ++configRevision; // open pages reload the settings: the Force unsafe toggle follows
        else
            Logger::error(TAG, "Couldn't save Force unsafe = %d", forceUnsafe ? 1 : 0);
    }

    void WebServer::syncManualHold(bool forceUnsafe)
    {
        std::string stored;
        {
            HoldsLock lock(holdsMutex);
            if (!Core::syncManualHold(holds, forceUnsafe, millis(), wallClock()))
                return;
            stored = holds.serialize();
        }
        storeHolds(stored);
    }

    WebServer::HoldsView WebServer::tickHolds(uint32_t now)
    {
        const SafetyStatus status = getSafetyStatus();
        HoldsView view;
        std::string stored;
        bool manualLifted = false;
        {
            HoldsLock lock(holdsMutex);
            const std::vector<Alpaca::SafetyHold> lifted = holds.update(now, wallClock());
            for (const Alpaca::SafetyHold &hold : lifted)
            {
                Logger::info(TAG, "Safety hold %s lifted at its time", hold.id.c_str());
                SafetyHistory::Entry entry{};
                Core::holdClearedEntry(entry, hold, status, holds);
                SafetyHistory::recordHold(entry);
                manualLifted = manualLifted || hold.manual();
            }
            if (!lifted.empty())
                stored = holds.serialize();
            if (holds.hasExternal())
            {
                view.reasons = holds.externalReasons();
                view.labels = holds.externalLabels(SafetyHistory::HOLD_TEXT_BYTES - 1);
            }
        }
        if (!stored.empty())
            storeHolds(stored);
        if (manualLifted)
            applyManualOverride(false);
        return view;
    }
} // namespace SQM
