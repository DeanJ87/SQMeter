#pragma once

#include <string>

namespace SQM
{
    namespace Alpaca
    {

        // ASCOM Alpaca standard error numbers (subset actually used here).
        // See the ASCOM Alpaca API spec / ASCOM.Common.Alpaca.AlpacaErrors.
        constexpr int ALPACA_ERR_NOT_IMPLEMENTED = 0x400;
        constexpr int ALPACA_ERR_INVALID_VALUE = 0x401;
        constexpr int ALPACA_ERR_NOT_CONNECTED = 0x407;
        constexpr int ALPACA_ERR_DRIVER_BASE = 0x500; // 0x500-0xFFF: custom driver errors

        struct PropertyResult
        {
            bool ok = false;
            double value = 0.0;
            int errorNumber = 0;
            std::string errorMessage;
        };

        // Sensor readings mapped into Alpaca's ObservingConditions property
        // space. `dataValid` reflects whether the underlying sensor data is
        // fresh/available - when false, every implemented property returns a
        // driver error instead of a stale/zeroed value.
        struct ObservingConditionsSnapshot
        {
            bool dataValid = false;
            float cloudCoverPercent = 0.0f;
            float dewpointC = 0.0f;
            float humidityPercent = 0.0f;
            float skyBrightnessLux = 0.0f;
            float skyQualityMagArcsec2 = 0.0f;
            float skyTemperatureC = 0.0f;
            float temperatureC = 0.0f;
        };

        // Case-insensitive Alpaca property name -> value/error. Properties
        // this device has no sensor for (pressure, rainrate, starfwhm,
        // winddirection, windgust, windspeed) return NotImplemented, matching
        // the Go bridge's documented behavior for the same gaps.
        // "averageperiod" is implemented and always 0 (no averaging done).
        PropertyResult getObservingConditionsProperty(const std::string &propertyName, const ObservingConditionsSnapshot &snapshot);

    } // namespace Alpaca
} // namespace SQM
