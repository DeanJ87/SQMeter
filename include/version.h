#pragma once

// Firmware version information
// Local builds: the latest release plus "+dev", so update checks order them
// correctly. CI stamps the tag (without "v") on release builds; bump this
// after each release.
#define FIRMWARE_VERSION "0.2.0-beta.1+dev"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__
#define FIRMWARE_NAME "SQMeter"

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
