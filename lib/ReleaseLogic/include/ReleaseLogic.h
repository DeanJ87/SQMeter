#pragma once

#include "ReleaseUrls.h"

#include <ArduinoJson.h>

#include <cstddef>
#include <string>
#include <vector>

namespace SQM
{
    struct GithubRelease
    {
        std::string tag;         // e.g. "v0.2.0-beta.1"
        std::string name;        // release title
        bool prerelease = false; // GitHub's native beta/stable flag
        std::string publishedAt;
        std::string firmwareAssetUrl; // sqmeter-l2-firmware-<tag>.bin (or sqmeter-l2-ble-firmware-)
        size_t firmwareAssetSize = 0;
        std::string fsAssetUrl; // sqmeter-l2-littlefs-<tag>.bin
        size_t fsAssetSize = 0;
    };

    namespace Releases
    {
        // Spec 027: files for the whole-chip partition layout ("l2"). Older
        // firmware looks for "sqmeter-firmware-" and friends, so it never
        // offers a release it would install into the wrong layout.
        constexpr const char *FIRMWARE_PREFIX = "sqmeter-l2-firmware-";
        constexpr const char *BLE_FIRMWARE_PREFIX = "sqmeter-l2-ble-firmware-";
        constexpr const char *FS_PREFIX = "sqmeter-l2-littlefs-";

        // The classic ESP32 keeps the names above; each ESP32-S3 board has its
        // own files (spec 034): sqmeter-<board>-firmware-, sqmeter-<board>-ble-firmware-
        // and sqmeter-<board>-littlefs-, so a device is only offered its own.
        constexpr const char *CLASSIC_BOARD = "esp32";
        struct AssetPrefixes
        {
            std::string firmware;
            std::string bleFirmware;
            std::string fs;
        };
        AssetPrefixes prefixesFor(const std::string &board);

        // The update check asks GitHub for this many releases...
        constexpr int PER_PAGE = 8;
        // ...and reads them (filtered to each file's name and size) into a
        // document this big: 32 KB on the ESP32. Since v0.3 every release also
        // carries ~27 language files and since v0.4 the ESP32-S3 boards' 15;
        // tested with 8 such releases of 51 files each (about 60 is the limit,
        // and firmware already in the field has the same capacity, so keep the
        // file count per release below that). ArduinoJson's slots grow with the pointer size, so the 64-bit
        // native tests get the same room in proportion.
        constexpr size_t JSON_CAPACITY = 8192 * sizeof(void *);

        // ArduinoJson filter keeping only the fields parse() reads; release
        // notes and uploader details are most of each release's JSON.
        void buildFilter(JsonDocument &filter);

        // GitHub "list releases" JSON -> the releases on `track` ("stable":
        // prerelease == false, "beta": prerelease == true) that have BOTH a
        // firmware image for this build (`ble` picks sqmeter-l2-ble-firmware-*)
        // and a sqmeter-l2-littlefs-* image. A release missing either is
        // skipped: firmware and web UI are always installed as a matched pair.
        // `board` is the build's release board (include/BuildTarget.h).
        std::vector<GithubRelease> parse(
            const JsonDocument &doc, const std::string &track, bool ble, const std::string &board = CLASSIC_BOARD);

        // Same, from the raw body. Returns false (and sets `error`) when the
        // JSON can't be read.
        bool parse(const std::string &json, const std::string &track, bool ble, std::vector<GithubRelease> &out, std::string &error);
    } // namespace Releases
} // namespace SQM
