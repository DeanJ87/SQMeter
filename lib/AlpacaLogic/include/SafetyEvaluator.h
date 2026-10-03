#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SQM
{
    namespace Alpaca
    {

        // Configurable safety thresholds, ported from the SQMeter-ASCOM-Alpaca
        // Go bridge's rule set (see its README "Safety rules" section) so the
        // firmware-native SafetyMonitor behaves identically to what it
        // replaces. Threshold checks are individually enable-able, matching
        // the Go bridge's "if configured" semantics - an unset threshold
        // never contributes to an unsafe verdict.
        struct SafetyThresholds
        {
            bool manualOverrideUnsafe = false;
            uint32_t staleAfterSeconds = 30;

            bool cloudCoverEnabled = true;
            float cloudCoverUnsafePercent = 90.0f;

            bool sqmMinEnabled = false;
            float sqmMinSafe = 0.0f;

            bool humidityMaxEnabled = false;
            float humidityMaxSafe = 100.0f;

            bool dewpointMarginEnabled = false;
            float dewpointMarginMinC = 0.0f;

            bool rainUnsafeEnabled = true;
            bool rainSensorRequired = true;

            bool windSpeedUnsafeEnabled = false;
            float windSpeedUnsafeMs = 10.0f;
            bool windGustUnsafeEnabled = false;
            float windGustUnsafeMs = 15.0f;
        };

        // Current sensor/data-freshness state to evaluate against the
        // thresholds above. Values are only consulted when the
        // corresponding threshold is enabled and data isn't stale/missing.
        struct SafetyInputs
        {
            bool hasEverHadGoodData = false;
            uint32_t secondsSinceLastGoodData = 0;
            bool requiredSensorFault = false; // any required sensor reporting a non-OK status
            // Which one: a faulted sensor's zeroed readings mustn't also
            // produce a misleading threshold reason (e.g. 100% cloud).
            bool skyLightFault = false; // TSL2591 -> SQM rule skipped
            bool irSkyFault = false;    // MLX90614 -> cloud cover rule skipped

            float cloudCoverPercent = 0.0f;
            float sqm = 0.0f;
            float humidityPercent = 0.0f;
            float temperatureC = 0.0f;
            float dewpointC = 0.0f;

            // BME280 not OK: humidity/dew point rules can't be evaluated.
            bool environmentSensorFault = false;

            // Rain sensor (RG-15). Evaluated independently of the freshness
            // of the other sensors above.
            bool rainSensorEnabled = false;
            bool rainSensorHealthy = false; // online, fresh, no lens fault
            bool raining = false;           // includes the post-rain hold-off latch

            // Anemometer. Like rain, evaluated independently of the others.
            bool windSensorEnabled = false;
            bool windSensorHealthy = false;
            float windSpeedMs = 0.0f;
            float windGustMs = 0.0f;
        };

        // One bit per distinct unsafe reason, so callers (alerts, BLE) can
        // detect which reasons appeared/cleared without string matching.
        enum UnsafeReasonFlag : uint32_t
        {
            UNSAFE_MANUAL_OVERRIDE = 1u << 0,
            UNSAFE_NO_DATA = 1u << 1,
            UNSAFE_STALE_DATA = 1u << 2,
            UNSAFE_SENSOR_FAULT = 1u << 3,
            UNSAFE_CLOUD_COVER = 1u << 4,
            UNSAFE_SKY_BRIGHT = 1u << 5,
            UNSAFE_HUMIDITY = 1u << 6,
            UNSAFE_DEWPOINT = 1u << 7,
            UNSAFE_ENVIRONMENT_FAULT = 1u << 8,
            UNSAFE_RAIN = 1u << 9,
            UNSAFE_RAIN_SENSOR_FAULT = 1u << 10,
            UNSAFE_WIND = 1u << 11,
            UNSAFE_WIND_GUST = 1u << 12,
            UNSAFE_WIND_SENSOR_FAULT = 1u << 13,
        };

        struct SafetyResult
        {
            bool isSafe = false;
            std::vector<std::string> unsafeReasons; // empty when isSafe is true
            uint32_t reasonFlags = 0;               // UnsafeReasonFlag bits, 0 when safe
        };

        // Pure evaluation, no I/O - matches the Go bridge's rule precedence:
        // manual override, then data freshness/availability, then sensor
        // health, then the individual threshold checks. All applicable
        // reasons are collected, not just the first one, so the diagnostics
        // endpoint can show everything that's wrong at once.
        SafetyResult evaluateSafety(const SafetyInputs &inputs, const SafetyThresholds &thresholds);

        // Holds a "safe" verdict back until conditions have been continuously
        // safe for delaySeconds, so a roof doesn't open on a momentary gap in
        // the clouds. Unsafe is always reported immediately. The timer also
        // starts at boot, so a reboot never reports safe early.
        class SafeDelayFilter
        {
        public:
            bool update(bool rawSafe, uint32_t nowSeconds, uint32_t delaySeconds);
            // Seconds left before a currently-safe raw verdict is reported
            // safe; 0 when already reported safe or currently unsafe.
            uint32_t secondsUntilSafe() const { return remaining; }

        private:
            bool rawSafeRunning = false;
            uint32_t safeSince = 0;
            uint32_t remaining = 0;
        };

    } // namespace Alpaca
} // namespace SQM
