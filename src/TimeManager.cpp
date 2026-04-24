#include "TimeManager.h"
#include "Logger.h"
#include <esp_sntp.h>
#include <Arduino.h>

namespace SQM
{
    TimeManager::TimeManager(const NTPConfig &ntpCfg, const GPSConfig &gpsCfg,
                             TimeSource primary, TimeSource secondary, GPSSensor *gps)
        : ntpConfig(ntpCfg),
          gpsConfig(gpsCfg),
          primarySource(primary),
          secondarySource(secondary),
          gpsSensor(gps),
          syncStatus(NTPSyncStatus::NOT_SYNCED),
          activeSource(ActiveTimeSource::NONE),
          lastSyncAttempt(0),
          lastSuccessfulSync(0),
          lastKnownTime(0)
    {
    }

    void TimeManager::begin()
    {
        Logger::info(TAG, "Initializing time synchronization");
        Logger::info(TAG, "Primary source: %s, Secondary: %s",
                     primarySource == TimeSource::NTP ? "NTP" : "GPS",
                     secondarySource == TimeSource::NTP ? "NTP" : "GPS");

        configureTimezone();

        // Initialize NTP if enabled
        if (ntpConfig.enabled)
        {
            Logger::info(TAG, "Configuring NTP: %s / %s", ntpConfig.server1.c_str(), ntpConfig.server2.c_str());
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, ntpConfig.server1.c_str());
            if (!ntpConfig.server2.empty())
            {
                sntp_setservername(1, ntpConfig.server2.c_str());
            }
            sntp_init();
        }

        syncStatus = NTPSyncStatus::IN_PROGRESS;
        lastSyncAttempt = millis();

        Logger::info(TAG, "Time synchronization started");
    }

    void TimeManager::handle()
    {
        const uint32_t now = millis();

        // Periodic sync attempts based on interval
        bool needsSync = false;
        if (syncStatus == NTPSyncStatus::SYNCED || syncStatus == NTPSyncStatus::FAILED)
        {
            uint32_t syncInterval = ntpConfig.enabled ? ntpConfig.syncIntervalMs : 300000; // 5 min default
            if (now - lastSyncAttempt >= syncInterval)
            {
                needsSync = true;
            }
        }

        // Check if current time is valid, if not try to sync immediately
        time_t currentTime;
        time(&currentTime);
        if (currentTime < 1000000000 && syncStatus != NTPSyncStatus::IN_PROGRESS)
        {
            needsSync = true;
        }

        if (needsSync)
        {
            syncTime();
        }

        // Check sync progress
        if (syncStatus == NTPSyncStatus::IN_PROGRESS)
        {
            time(&currentTime);
            if (currentTime > 1000000000) // Time is set
            {
                syncStatus = NTPSyncStatus::SYNCED;
                lastSuccessfulSync = now;
                lastKnownTime = currentTime;
                Logger::info(TAG, "Time sync successful via %s",
                             activeSource == ActiveTimeSource::NTP ? "NTP" : "GPS");
            }
            else if (now - lastSyncAttempt > 30000) // 30 second timeout
            {
                syncStatus = NTPSyncStatus::FAILED;
                activeSource = ActiveTimeSource::NONE;
                Logger::warn(TAG, "Time sync timeout");
            }
        }
    }

    void TimeManager::syncTime()
    {
        Logger::info(TAG, "Attempting time sync");
        syncStatus = NTPSyncStatus::IN_PROGRESS;
        lastSyncAttempt = millis();
        activeSource = ActiveTimeSource::NONE; // Clear until source succeeds

        // Try primary source first, then fallback to secondary
        if (tryTimeSource(primarySource))
        {
            return; // Primary initiated/succeeded
        }

        Logger::warn(TAG, "Primary time source failed, trying secondary");
        if (!tryTimeSource(secondarySource))
        {
            Logger::error(TAG, "Both time sources failed");
            syncStatus = NTPSyncStatus::FAILED;
            activeSource = ActiveTimeSource::NONE;
        }
    }

    bool TimeManager::tryTimeSource(TimeSource source)
    {
        if (source == TimeSource::NTP && ntpConfig.enabled)
        {
            return syncFromNTP();
        }
        else if (source == TimeSource::GPS && gpsConfig.enabled && gpsSensor)
        {
            return syncFromGPS();
        }
        return false;
    }

    bool TimeManager::syncFromNTP()
    {
        Logger::info(TAG, "Attempting sync from NTP");
        sntp_stop();
        sntp_init();
        // NTP sync is async - will be verified in handle() when time > 1000000000
        // Set activeSource as pending NTP
        activeSource = ActiveTimeSource::NTP;
        return true; // NTP started successfully
    }

    bool TimeManager::syncFromGPS()
    {
        if (!gpsSensor)
        {
            Logger::error(TAG, "GPS sensor not available");
            return false;
        }

        GPSReading reading = gpsSensor->getReading();
        if (!reading.hasFix)
        {
            Logger::warn(TAG, "GPS has no fix, cannot sync time");
            return false;
        }

        if (!gpsSensor->hasValidTime() || !gpsSensor->hasValidDate())
        {
            Logger::warn(TAG, "GPS time/date not valid");
            return false;
        }

        struct tm timeinfo;
        if (!gpsSensor->getDateTime(&timeinfo))
        {
            Logger::error(TAG, "Failed to get GPS time");
            return false;
        }

        // Set system time from GPS
        time_t gpsTime = mktime(&timeinfo);
        struct timeval tv = {.tv_sec = gpsTime, .tv_usec = 0};
        settimeofday(&tv, nullptr);

        activeSource = ActiveTimeSource::GPS;
        syncStatus = NTPSyncStatus::SYNCED;
        lastSuccessfulSync = millis();

        Logger::info(TAG, "Time synced from GPS");
        return true;
    }

    void TimeManager::configureTimezone()
    {
        Logger::info(TAG, "Setting timezone: %s", ntpConfig.timezone.c_str());
        setenv("TZ", ntpConfig.timezone.c_str(), 1);
        tzset();
    }

    void TimeManager::updateConfig(const NTPConfig &newNtpConfig, const GPSConfig &newGpsConfig,
                                   TimeSource newPrimary, TimeSource newSecondary)
    {
        bool serverChanged = (ntpConfig.server1 != newNtpConfig.server1) ||
                             (ntpConfig.server2 != newNtpConfig.server2);
        bool tzChanged = (ntpConfig.timezone != newNtpConfig.timezone);
        bool priorityChanged = (primarySource != newPrimary) || (secondarySource != newSecondary);

        ntpConfig = newNtpConfig;
        gpsConfig = newGpsConfig;
        primarySource = newPrimary;
        secondarySource = newSecondary;

        if (tzChanged)
        {
            configureTimezone();
        }

        if ((serverChanged && ntpConfig.enabled) || priorityChanged)
        {
            Logger::info(TAG, "Time config changed, re-synchronizing");
            if (ntpConfig.enabled)
            {
                sntp_stop();
            }
            // Trigger immediate re-sync with new config
            syncTime();
        }
    }

    TimeStatus TimeManager::getStatus() const
    {
        TimeStatus status;
        status.ntpEnabled = ntpConfig.enabled;
        status.gpsEnabled = gpsConfig.enabled;
        status.syncStatus = syncStatus;
        status.activeSource = activeSource;
        status.lastSyncMs = lastSuccessfulSync;
        status.server = ntpConfig.server1;
        status.timezone = ntpConfig.timezone;

        // GPS status
        if (gpsSensor)
        {
            GPSReading reading = gpsSensor->getReading();
            status.gpsHasFix = reading.hasFix;
            status.gpsTimeSinceUpdate = gpsSensor->getTimeSinceLastUpdate();
            status.gpsSatellites = reading.satellites;

            // Get GPS UTC time if available
            if (gpsSensor->hasValidTime() && gpsSensor->hasValidDate())
            {
                struct tm gpsTime;
                if (gpsSensor->getDateTime(&gpsTime))
                {
                    char buffer[32];
                    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &gpsTime);
                    status.gpsTimeUTC = std::string(buffer);
                }
                else
                {
                    status.gpsTimeUTC = "Invalid";
                }
            }
            else
            {
                status.gpsTimeUTC = "No GPS time";
            }
        }
        else
        {
            status.gpsHasFix = false;
            status.gpsTimeSinceUpdate = 0xFFFFFFFF;
            status.gpsSatellites = 0;
            status.gpsTimeUTC = "GPS disabled";
        }

        const uint32_t now = millis();
        if (syncStatus == NTPSyncStatus::SYNCED)
        {
            uint32_t syncInterval = ntpConfig.enabled ? ntpConfig.syncIntervalMs : 300000;
            uint32_t timeSinceSync = now - lastSuccessfulSync;
            if (timeSinceSync < syncInterval)
            {
                status.nextSyncMs = syncInterval - timeSinceSync;
            }
            else
            {
                status.nextSyncMs = 0; // Overdue for sync
            }
        }
        else
        {
            status.nextSyncMs = 0;
        }

        status.driftSeconds = 0;

        return status;
    }

    std::string TimeManager::getCurrentTimeISO() const
    {
        time_t now;
        time(&now);

        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);

        return std::string(buffer);
    }

    time_t TimeManager::getCurrentTime() const
    {
        time_t now;
        time(&now);
        return now;
    }

} // namespace SQM
