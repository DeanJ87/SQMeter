#pragma once

// Firmware version information
#define FIRMWARE_VERSION "0.0.1"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__
#define FIRMWARE_NAME "SQMv2"

// Helper to get full version string
inline const char *getFirmwareVersion()
{
    static char version[64];
    snprintf(version, sizeof(version), "%s v%s", FIRMWARE_NAME, FIRMWARE_VERSION);
    return version;
}

// Helper to get build timestamp
inline const char *getBuildTimestamp()
{
    static char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%s %s", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    return timestamp;
}
