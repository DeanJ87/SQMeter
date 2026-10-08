#pragma once

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
        std::string firmwareAssetUrl; // sqmeter-firmware-<tag>.bin (or sqmeter-ble-firmware-)
        size_t firmwareAssetSize = 0;
        std::string fsAssetUrl; // sqmeter-littlefs-<tag>.bin
        size_t fsAssetSize = 0;
    };

    namespace Releases
    {
        // The update check asks GitHub for this many releases...
        constexpr int PER_PAGE = 8;
        // ...and reads them (filtered) into a document this big. Each release
        // now ships 5 files; 8 of them need ~7 KB on the ESP32, so 6 KB
        // stopped working at v0.2.0-beta.2. Tested with 8 releases x 6 files.
        constexpr size_t JSON_CAPACITY = 16384;

        // ArduinoJson filter keeping only the fields parse() reads; release
        // notes and uploader details are most of each release's JSON.
        void buildFilter(JsonDocument &filter);

        // GitHub "list releases" JSON -> the releases on `track` ("stable":
        // prerelease == false, "beta": prerelease == true) that have BOTH a
        // firmware image for this build (`ble` picks sqmeter-ble-firmware-*)
        // and a sqmeter-littlefs-* image. A release missing either is skipped:
        // firmware and web UI are always installed as a matched pair.
        std::vector<GithubRelease> parse(const JsonDocument &doc, const std::string &track, bool ble);

        // Same, from the raw body. Returns false (and sets `error`) when the
        // JSON can't be read.
        bool parse(const std::string &json, const std::string &track, bool ble, std::vector<GithubRelease> &out, std::string &error);
    } // namespace Releases
} // namespace SQM
