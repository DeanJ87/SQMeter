#pragma once

#include <cstdint>

namespace SQM
{
    // When a sky sensor (TSL2591) is started, read and re-begun (spec 030,
    // research D2). Pure timing and retry state: the driver does the I2C and
    // reports back, so two sensors can integrate at the same time without the
    // loop waiting for either.
    //
    //   Idle --due--> Integrating --integration + margin--> collect --> Idle
    //   3 failed reads in a row --> Absent; Absent retries begin() with a
    //   back-off of 5 s doubling to 60 s; a sensor never begun tries at once.
    class SkyReadCycle
    {
    public:
        enum class State : uint8_t
        {
            Idle,
            Integrating,
            Absent,
        };

        // What the driver should do now.
        enum class Action : uint8_t
        {
            None,
            Begin,   // (re)initialise the sensor; report with beginDone()
            Start,   // enable the ADC; report with started() or startFailed()
            Collect, // read the channels; report with collected()
        };

        static constexpr uint32_t MARGIN_MS = 20;
        // How long after the start the counts are ready: the integration
        // plus 20% (the TSL2591's internal clock tolerance, as the Adafruit
        // library allows) and a margin.
        static constexpr uint32_t collectDelayMs(uint16_t integrationMs) { return integrationMs + integrationMs / 5 + MARGIN_MS; }
        static constexpr uint8_t REBEGIN_AFTER = 3;
        static constexpr uint32_t BACKOFF_FIRST_MS = 5000;
        static constexpr uint32_t BACKOFF_MAX_MS = 60000;

        explicit SkyReadCycle(uint32_t intervalMs)
            : interval(intervalMs)
        {
        }

        Action next(uint32_t nowMs, uint16_t integrationMs) const;

        void beginDone(uint32_t nowMs, bool ok);
        void started(uint32_t nowMs);
        void startFailed(uint32_t nowMs);
        void collected(uint32_t nowMs, bool ok);

        State state() const { return current; }
        // Begun and answering (not Absent).
        bool initialized() const { return current != State::Absent; }
        uint8_t consecutiveFailures() const { return failures; }

    private:
        void failed(uint32_t nowMs);
        static bool reached(uint32_t nowMs, uint32_t atMs) { return static_cast<int32_t>(nowMs - atMs) >= 0; }

        uint32_t interval;
        State current = State::Absent;
        bool startedOnce = false; // a start since the last begin
        uint32_t lastStartMs = 0;
        uint32_t retryAtMs = 0;
        bool retryNow = true; // never begun: try at once
        uint32_t backoffMs = BACKOFF_FIRST_MS;
        uint8_t failures = 0;
    };
} // namespace SQM
