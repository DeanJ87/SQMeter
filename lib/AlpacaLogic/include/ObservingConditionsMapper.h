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

        struct StringResult
        {
            bool ok = false;
            std::string value;
            int errorNumber = 0;
            std::string errorMessage;
        };

        // State of one physical sensor feeding ObservingConditions.
        // `present` = the hardware is fitted/enabled at all (a property whose
        // source isn't present is NotImplemented); `valid` = its latest
        // reading is fresh and healthy (otherwise the property returns a
        // driver error rather than a stale/zeroed value).
        struct SourceState
        {
            bool present = false;
            bool valid = false;
            double ageSeconds = 0.0; // since the last successful reading
        };

        // Sensor readings mapped into Alpaca's ObservingConditions property
        // space, in Alpaca units, with per-sensor validity so one failed
        // sensor doesn't knock out properties served by the others.
        struct ObservingConditionsSnapshot
        {
            SourceState skyLight;    // TSL2591: skybrightness, skyquality
            SourceState irSky;       // MLX90614: skytemperature, cloudcover
            SourceState environment; // BME280: temperature, humidity, dewpoint, pressure
            SourceState rain;        // RG-15: rainrate
            SourceState wind;        // anemometer: windspeed, windgust
            SourceState windVane;    // wind vane: winddirection

            float cloudCoverPercent = 0.0f;
            float dewpointC = 0.0f;
            float humidityPercent = 0.0f;
            float pressureHPa = 0.0f;
            float rainRateMmPerHour = 0.0f;
            float skyBrightnessLux = 0.0f;
            float skyQualityMagArcsec2 = 0.0f;
            float skyTemperatureC = 0.0f;
            float temperatureC = 0.0f;
            float windDirectionDeg = 0.0f;
            float windGustMs = 0.0f;
            float windSpeedMs = 0.0f;
        };

        // Case-insensitive Alpaca property name -> value/error. starfwhm is
        // never implemented (it needs a camera); other properties are
        // NotImplemented when their sensor isn't present.
        // "averageperiod" is implemented and always 0 (no averaging done).
        PropertyResult getObservingConditionsProperty(const std::string &propertyName, const ObservingConditionsSnapshot &snapshot);

        // SensorDescription(SensorName): which physical sensor serves a property.
        StringResult getSensorDescription(const std::string &sensorName, const ObservingConditionsSnapshot &snapshot);

        // TimeSinceLastUpdate(SensorName), in seconds. An empty name means
        // the most recent update from any present sensor.
        PropertyResult getTimeSinceLastUpdate(const std::string &sensorName, const ObservingConditionsSnapshot &snapshot);

        // PUT AveragePeriod: only 0 (instantaneous) is supported.
        PropertyResult validateAveragePeriod(double hours);

        // RG-15 rain intensity -> Alpaca's mm/h.
        float rainRateToMmPerHour(float rate, bool imperial);

    } // namespace Alpaca
} // namespace SQM
