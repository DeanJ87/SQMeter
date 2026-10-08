#include "ObservingConditionsMapper.h"
#include <algorithm>
#include <cctype>

namespace SQM
{
    namespace Alpaca
    {
        namespace
        {
            std::string toLower(const std::string &s)
            {
                std::string out = s;
                std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
                return out;
            }

            PropertyResult error(int number, const std::string &message)
            {
                PropertyResult r;
                r.ok = false;
                r.errorNumber = number;
                r.errorMessage = message;
                return r;
            }

            PropertyResult notImplemented()
            {
                return error(ALPACA_ERR_NOT_IMPLEMENTED, "Property not implemented by this device");
            }

            PropertyResult noData()
            {
                return error(ALPACA_ERR_DRIVER_BASE, "No valid sensor data available");
            }

            PropertyResult value(double v)
            {
                PropertyResult r;
                r.ok = true;
                r.value = v;
                return r;
            }

            struct PropertyInfo
            {
                const char *name;
                const SourceState ObservingConditionsSnapshot::*source; // nullptr = never implemented
                const char *description;
            };

            constexpr PropertyInfo PROPERTIES[] = {
                {"cloudcover", &ObservingConditionsSnapshot::irSky, "MLX90614 IR sky-minus-ambient temperature, humidity corrected"},
                {"dewpoint", &ObservingConditionsSnapshot::environment, "BME280 temperature and humidity (Magnus formula)"},
                {"humidity", &ObservingConditionsSnapshot::environment, "BME280 relative humidity"},
                {"pressure", &ObservingConditionsSnapshot::environment, "BME280 barometric pressure (station level)"},
                {"rainrate", &ObservingConditionsSnapshot::rain, "Hydreon RG-15 optical rain gauge"},
                {"skybrightness", &ObservingConditionsSnapshot::skyLight, "TSL2591 light sensor"},
                {"skyquality", &ObservingConditionsSnapshot::skyLight, "TSL2591 light sensor, calibrated SQM"},
                {"skytemperature", &ObservingConditionsSnapshot::irSky, "MLX90614 IR thermometer"},
                {"starfwhm", nullptr, ""},
                {"temperature", &ObservingConditionsSnapshot::environment, "BME280 ambient temperature"},
                {"winddirection", &ObservingConditionsSnapshot::windVane, "Resistor-ladder wind vane, speed-weighted 2 min mean"},
                {"windgust", &ObservingConditionsSnapshot::wind, "Cup anemometer, peak 3 s mean over 10 min"},
                {"windspeed", &ObservingConditionsSnapshot::wind, "Cup anemometer, 2 min mean"},
            };

            const PropertyInfo *findProperty(const std::string &lowerName)
            {
                for (const PropertyInfo &info : PROPERTIES)
                {
                    if (lowerName == info.name)
                        return &info;
                }
                return nullptr;
            }
        } // namespace

        PropertyResult getObservingConditionsProperty(const std::string &propertyName, const ObservingConditionsSnapshot &snapshot)
        {
            const std::string name = toLower(propertyName);

            if (name == "averageperiod")
                return value(0.0); // no averaging performed

            const PropertyInfo *info = findProperty(name);
            if (info == nullptr || info->source == nullptr)
                return notImplemented();

            const SourceState &source = snapshot.*(info->source);
            if (!source.present)
                return notImplemented();
            if (!source.valid)
                return noData();

            if (name == "cloudcover")
                return value(snapshot.cloudCoverPercent);
            if (name == "dewpoint")
                return value(snapshot.dewpointC);
            if (name == "humidity")
                return value(snapshot.humidityPercent);
            if (name == "pressure")
                return value(snapshot.pressureHPa);
            if (name == "rainrate")
                return value(snapshot.rainRateMmPerHour);
            if (name == "skybrightness")
                return value(snapshot.skyBrightnessLux);
            if (name == "skyquality")
                return value(snapshot.skyQualityMagArcsec2);
            if (name == "skytemperature")
                return value(snapshot.skyTemperatureC);
            if (name == "temperature")
                return value(snapshot.temperatureC);
            if (name == "winddirection")
                // Alpaca: direction is 0 when there is no wind.
                return value(snapshot.windSpeedMs > 0.0f ? snapshot.windDirectionDeg : 0.0);
            if (name == "windgust")
                return value(snapshot.windGustMs);
            if (name == "windspeed")
                return value(snapshot.windSpeedMs);

            return notImplemented();
        }

        StringResult getSensorDescription(const std::string &sensorName, const ObservingConditionsSnapshot &snapshot)
        {
            StringResult r;
            const PropertyInfo *info = findProperty(toLower(sensorName));
            if (info == nullptr)
            {
                r.errorNumber = ALPACA_ERR_INVALID_VALUE;
                r.errorMessage = "Unknown sensor name: " + sensorName;
                return r;
            }
            if (info->source == nullptr || !(snapshot.*(info->source)).present)
            {
                r.errorNumber = ALPACA_ERR_NOT_IMPLEMENTED;
                r.errorMessage = "Sensor not implemented by this device";
                return r;
            }
            r.ok = true;
            r.value = info->description;
            return r;
        }

        PropertyResult getTimeSinceLastUpdate(const std::string &sensorName, const ObservingConditionsSnapshot &snapshot)
        {
            if (sensorName.empty())
            {
                bool any = false;
                double youngest = 0.0;
                for (const SourceState *source :
                     {&snapshot.skyLight, &snapshot.irSky, &snapshot.environment, &snapshot.rain, &snapshot.wind, &snapshot.windVane})
                {
                    if (!source->present || !source->valid)
                        continue;
                    youngest = any ? std::min(youngest, source->ageSeconds) : source->ageSeconds;
                    any = true;
                }
                return any ? value(youngest) : noData();
            }

            const PropertyInfo *info = findProperty(toLower(sensorName));
            if (info == nullptr)
                return error(ALPACA_ERR_INVALID_VALUE, "Unknown sensor name: " + sensorName);
            if (info->source == nullptr)
                return notImplemented();
            const SourceState &source = snapshot.*(info->source);
            if (!source.present)
                return notImplemented();
            if (!source.valid)
                return noData();
            return value(source.ageSeconds);
        }

        PropertyResult validateAveragePeriod(double hours)
        {
            if (hours == 0.0)
                return value(0.0);
            return error(ALPACA_ERR_INVALID_VALUE, "Only an AveragePeriod of 0 (instantaneous readings) is supported");
        }

        float rainRateToMmPerHour(float rate, bool imperial)
        {
            return imperial ? rate * 25.4f : rate;
        }

    } // namespace Alpaca
} // namespace SQM
