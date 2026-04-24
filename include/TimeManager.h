#pragma once

#include "Config.h"
#include "sensors/GPSSensor.h"
#include <time.h>
#include <sys/time.h>
#include <cstdint>
#include <string>

namespace SQM
{
    enum class NTPSyncStatus
    {
        NOT_SYNCED = 0,
        IN_PROGRESS = 1,
        SYNCED = 2,
        FAILED = 3
    };

    enum class ActiveTimeSource
    {
        NONE = 0,
        NTP = 1,
        GPS = 2
    };

    struct TimeStatus
    {
        bool ntpEnabled;
        bool gpsEnabled;
        NTPSyncStatus syncStatus;
        ActiveTimeSource activeSource;
        uint32_t lastSyncMs;
        uint32_t nextSyncMs;
        int32_t driftSeconds;
        std::string server;
        std::string timezone;
        bool gpsHasFix;
        uint32_t gpsTimeSinceUpdate;
        std::string gpsTimeUTC; // GPS time in ISO format (UTC)
        uint32_t gpsSatellites;
    };

    class TimeManager
    {
    public:
        TimeManager(const NTPConfig &ntpCfg, const GPSConfig &gpsCfg,
                    TimeSource primary, TimeSource secondary, GPSSensor *gps);
        ~TimeManager() = default;

        void begin();
        void handle();
        void updateConfig(const NTPConfig &newNtpConfig, const GPSConfig &newGpsConfig,
                          TimeSource newPrimary, TimeSource newSecondary);

        TimeStatus getStatus() const;
        std::string getCurrentTimeISO() const;
        time_t getCurrentTime() const;
        ActiveTimeSource getActiveTimeSource() const { return activeSource; }

    private:
        static constexpr const char *TAG = "TimeManager";

        NTPConfig ntpConfig;
        GPSConfig gpsConfig;
        TimeSource primarySource;
        TimeSource secondarySource;
        GPSSensor *gpsSensor;

        NTPSyncStatus syncStatus;
        ActiveTimeSource activeSource;
        uint32_t lastSyncAttempt;
        uint32_t lastSuccessfulSync;
        time_t lastKnownTime;

        void syncTime();
        bool syncFromNTP();
        bool syncFromGPS();
        void configureTimezone();
        bool tryTimeSource(TimeSource source);
    };

} // namespace SQM
