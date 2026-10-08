#pragma once

#include <cstddef>
#include <cstdint>

namespace SQM
{
    namespace Wind
    {

        // Cup-anemometer speed per pulse frequency (km/h per Hz).
        constexpr float MISOL_KMH_PER_HZ = 2.4f;   // Misol WH-SP-WS01 / Argent / SparkFun weather meter
        constexpr float DAVIS_KMH_PER_HZ = 3.621f; // Davis 6410: 2.25 mph per Hz

        // Aggregates one-second anemometer/vane samples into the values
        // ObservingConditions reports:
        //  - speed:     mean over the last 2 minutes (as METAR/AWOS report it)
        //  - gust:      highest 3-second mean in the last 10 minutes (WMO)
        //  - direction: speed-weighted circular mean over the last 2 minutes
        class WindAggregator
        {
        public:
            static constexpr size_t HISTORY_SECONDS = 600;
            static constexpr size_t SPEED_WINDOW_SECONDS = 120;
            static constexpr size_t GUST_WINDOW_SECONDS = 3;

            // speedMs: that second's mean speed. directionDeg < 0 = no vane reading.
            void addSample(float speedMs, float directionDeg);
            void reset();

            size_t sampleCount() const { return count; }
            float speedMs() const;
            float gustMs() const;
            // Returns false when there's no direction data (no vane, or calm).
            bool directionDeg(float &out) const;

        private:
            // Stored as 16-bit fixed point (2.4 KB instead of 4.8 KB):
            // speed in cm/s, direction in 0.1 degree, NO_DIRECTION = none.
            static constexpr uint16_t NO_DIRECTION = 0xFFFF;
            float speedAt(size_t index) const { return speeds[index] / 100.0f; }

            uint16_t speeds[HISTORY_SECONDS] = {};
            uint16_t directions[HISTORY_SECONDS] = {};
            size_t head = 0; // next write index
            size_t count = 0;

            // i = 0 is the newest sample
            size_t indexBack(size_t i) const { return (head + HISTORY_SECONDS - 1 - i) % HISTORY_SECONDS; }
        };

        // Misol/Argent-style resistor-ladder vane with a pull-up to the ADC
        // reference: maps the measured ratio V/Vref to the nearest of its 16
        // directions. Returns false when the ratio matches nothing (open or
        // shorted vane).
        bool vaneDirectionFromRatio(float ratio, float pullupOhms, float &directionDeg);

    } // namespace Wind
} // namespace SQM
