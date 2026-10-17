#include "DeviceCore.h"

#include <cstdio>

// The Bluetooth summary characteristic's JSON (src/WebServerAlerts.cpp
// updateBle): readings with two decimals, only valid ones (spec 030 adds
// sqmSource, spec 032 the battery).

namespace SQM::Core
{
    void writeBleSummary(JsonObject out, const Alpaca::ObservingConditionsSnapshot &obs, const Readings::Battery &battery)
    {
        auto put = [&out](const char *key, bool valid, float value)
        {
            if (!valid)
                return;
            char text[16];
            std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(value));
            out[key] = serialized(std::string(text)); // copied
        };
        put("sqm", obs.skyLight.valid, obs.skyQualityMagArcsec2);
        if (obs.skyLight.valid)
            out["sqmSource"] = skyRoleName(obs.skyRole); // which sky sensor `sqm` is
        put("cloud", obs.irSky.valid, obs.cloudCoverPercent);
        put("skyT", obs.irSky.valid, obs.skyTemperatureC);
        put("temp", obs.environment.valid, obs.temperatureC);
        put("hum", obs.environment.valid, obs.humidityPercent);
        put("dew", obs.environment.valid, obs.dewpointC);
        put("press", obs.environment.valid, obs.pressureHPa);
        put("wind", obs.wind.valid, obs.windSpeedMs);
        put("gust", obs.wind.valid, obs.windGustMs);
        if (battery.present && battery.status == Readings::Status::Ok && battery.percent)
            out["battery"] = static_cast<int>(*battery.percent + 0.5);
        if (battery.present && battery.state != nullptr)
            out["batteryState"] = battery.state;
    }
} // namespace SQM::Core
