#include "ReleaseLogic.h"

#include <cstring>

namespace SQM
{
    namespace Releases
    {
        namespace
        {

            // The URL is built from the tag and file name rather than read from
            // GitHub's JSON: dropping browser_download_url halves the memory
            // each file costs in the filtered document.
            bool findAsset(JsonArrayConst assets, const std::string &tag, const char *prefix, std::string &url, size_t &size)
            {
                const size_t prefixLen = strlen(prefix);
                for (JsonObjectConst asset : assets)
                {
                    const char *name = asset["name"] | "";
                    const size_t len = strlen(name);
                    if (strncmp(name, prefix, prefixLen) == 0 && len > 4 && strcmp(name + len - 4, ".bin") == 0)
                    {
                        url = std::string(DOWNLOAD_BASE) + tag + "/" + name;
                        size = asset["size"] | 0;
                        return true;
                    }
                }
                return false;
            }
        } // namespace

        void buildFilter(JsonDocument &filter)
        {
            JsonObject release = filter.createNestedObject();
            release["tag_name"] = true;
            release["name"] = true;
            release["prerelease"] = true;
            release["draft"] = true;
            release["published_at"] = true;
            JsonObject asset = release["assets"].createNestedObject();
            asset["name"] = true;
            asset["size"] = true;
        }

        std::vector<GithubRelease> parse(const JsonDocument &doc, const std::string &track, bool ble)
        {
            std::vector<GithubRelease> results;
            const bool wantPrerelease = (track == "beta");
            // The BLE build ships as its own file; installing the standard
            // firmware would silently drop Bluetooth (and the device refuses it).
            const char *firmwarePrefix = ble ? BLE_FIRMWARE_PREFIX : FIRMWARE_PREFIX;

            for (JsonObjectConst release : doc.as<JsonArrayConst>())
            {
                const bool prerelease = release["prerelease"] | false;
                if (prerelease != wantPrerelease || (release["draft"] | false))
                    continue;

                GithubRelease entry;
                entry.tag = std::string(release["tag_name"] | "");
                if (entry.tag.empty())
                    continue;
                entry.name = std::string(release["name"] | entry.tag.c_str());
                if (entry.name.empty())
                    entry.name = entry.tag;
                entry.prerelease = prerelease;
                entry.publishedAt = std::string(release["published_at"] | "");

                JsonArrayConst assets = release["assets"].as<JsonArrayConst>();
                const bool hasFirmware = findAsset(assets, entry.tag, firmwarePrefix, entry.firmwareAssetUrl, entry.firmwareAssetSize);
                const bool hasFs = findAsset(assets, entry.tag, FS_PREFIX, entry.fsAssetUrl, entry.fsAssetSize);
                if (!hasFirmware || !hasFs)
                    continue;

                results.push_back(std::move(entry));
            }
            return results;
        }

        bool parse(const std::string &json, const std::string &track, bool ble, std::vector<GithubRelease> &out, std::string &error)
        {
            StaticJsonDocument<512> filter; // 256 is too small on 64-bit hosts
            buildFilter(filter);
            DynamicJsonDocument doc(JSON_CAPACITY);
            const DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
            if (err)
            {
                error = std::string("Couldn't read the GitHub releases list: ") + err.c_str();
                out.clear();
                return false;
            }
            out = parse(doc, track, ble);
            return true;
        }
    } // namespace Releases
} // namespace SQM
