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
        };

        // Current sensor/data-freshness state to evaluate against the
        // thresholds above. Values are only consulted when the
        // corresponding threshold is enabled and data isn't stale/missing.
        struct SafetyInputs
        {
            bool hasEverHadGoodData = false;
            uint32_t secondsSinceLastGoodData = 0;
            bool requiredSensorFault = false; // any required sensor reporting a non-OK status

            float cloudCoverPercent = 0.0f;
            float sqm = 0.0f;
            float humidityPercent = 0.0f;
            float temperatureC = 0.0f;
            float dewpointC = 0.0f;
        };

        struct SafetyResult
        {
            bool isSafe = false;
            std::vector<std::string> unsafeReasons; // empty when isSafe is true
        };

        // Pure evaluation, no I/O - matches the Go bridge's rule precedence:
        // manual override, then data freshness/availability, then sensor
        // health, then the individual threshold checks. All applicable
        // reasons are collected, not just the first one, so the diagnostics
        // endpoint can show everything that's wrong at once.
        SafetyResult evaluateSafety(const SafetyInputs &inputs, const SafetyThresholds &thresholds);

    } // namespace Alpaca
} // namespace SQM
