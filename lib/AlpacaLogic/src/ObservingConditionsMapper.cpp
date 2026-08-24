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
                std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
                                { return std::tolower(c); });
                return out;
            }

            PropertyResult notImplemented()
            {
                PropertyResult r;
                r.ok = false;
                r.errorNumber = ALPACA_ERR_NOT_IMPLEMENTED;
                r.errorMessage = "Property not implemented by this device";
                return r;
            }

            PropertyResult noData()
            {
                PropertyResult r;
                r.ok = false;
                r.errorNumber = ALPACA_ERR_DRIVER_BASE;
                r.errorMessage = "No valid sensor data available";
                return r;
            }

            PropertyResult value(double v)
            {
                PropertyResult r;
                r.ok = true;
                r.value = v;
                return r;
            }
        }

        PropertyResult getObservingConditionsProperty(const std::string &propertyName, const ObservingConditionsSnapshot &snapshot)
        {
            const std::string name = toLower(propertyName);

            if (name == "averageperiod")
            {
                return value(0.0); // no averaging performed
            }

            if (name == "pressure" || name == "rainrate" || name == "starfwhm" ||
                name == "winddirection" || name == "windgust" || name == "windspeed")
            {
                return notImplemented();
            }

            if (!snapshot.dataValid)
            {
                return noData();
            }

            if (name == "cloudcover")
                return value(snapshot.cloudCoverPercent);
            if (name == "dewpoint")
                return value(snapshot.dewpointC);
            if (name == "humidity")
                return value(snapshot.humidityPercent);
            if (name == "skybrightness")
                return value(snapshot.skyBrightnessLux);
            if (name == "skyquality")
                return value(snapshot.skyQualityMagArcsec2);
            if (name == "skytemperature")
                return value(snapshot.skyTemperatureC);
            if (name == "temperature")
                return value(snapshot.temperatureC);

            return notImplemented();
        }

    } // namespace Alpaca
} // namespace SQM
