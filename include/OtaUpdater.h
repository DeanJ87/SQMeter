#pragma once

#include <string>
#include <vector>
#include <functional>

namespace SQM
{

    struct GithubRelease
    {
        std::string tag;         // e.g. "v0.0.3"
        std::string name;        // release title
        bool prerelease = false; // GitHub's native beta/stable flag
        std::string publishedAt;
        std::string firmwareAssetUrl; // browser_download_url for sqmeter-firmware-<tag>.bin
        size_t firmwareAssetSize = 0;
        std::string fsAssetUrl; // browser_download_url for sqmeter-littlefs-<tag>.bin
        size_t fsAssetSize = 0;
    };

    // Parses a GitHub "list releases" API JSON body and returns entries that
    // have BOTH a sqmeter-firmware-*.bin and a sqmeter-littlefs-*.bin asset,
    // filtered by track ("stable" -> prerelease == false, "beta" ->
    // prerelease == true). A release missing either asset is skipped
    // entirely - firmware and web UI must always be flashed as a matched
    // pair to avoid frontend/backend drift.
    // Exposed standalone (no networking) so it's unit-testable natively.
    std::vector<GithubRelease> parseGithubReleases(const std::string &json, const std::string &track);

    class OtaUpdater
    {
    public:
        enum class Phase
        {
            Idle,
            Checking,
            Downloading,
            Writing,
            Done,
            Error
        };

        using ProgressCallback = std::function<void(int percent)>;
        using ErrorCallback = std::function<void(const char *message)>;
        using RestartCallback = std::function<void()>;

        OtaUpdater(ProgressCallback onProgress, ErrorCallback onError, RestartCallback onRestart);

        // Blocking HTTPS GET against the GitHub releases API. Safe to call
        // from a request handler - completes in a couple seconds.
        std::vector<GithubRelease> checkForUpdate(const std::string &track, std::string &error);

        // Starts the download+flash of BOTH the firmware and filesystem
        // assets on its own FreeRTOS task so the caller (an ESPAsyncWebServer
        // request handler) returns immediately instead of blocking the web
        // server for the duration of the download. Only reboots after both
        // have been written successfully, so the device never runs a
        // mismatched firmware/web-UI pair.
        // Returns false if an update is already in progress.
        bool applyUpdate(const GithubRelease &release);

        Phase phase() const { return currentPhase; }

    private:
        void runApply(GithubRelease release);
        bool downloadAndFlashFirmware(const std::string &url, size_t expectedSize, int progressFrom, int progressTo);
        bool downloadAndFlashFilesystem(const std::string &url, size_t expectedSize, int progressFrom, int progressTo);

        ProgressCallback progressCb;
        ErrorCallback errorCb;
        RestartCallback restartCb;
        volatile Phase currentPhase = Phase::Idle;
    };

} // namespace SQM
