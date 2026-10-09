#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// The device's language choice and the language files it may store
// (specs/023-i18n FR-009..FR-013). Pure: the download and storage live in
// src/LanguagePack.

namespace SQM
{
    namespace Language
    {
        // Keep in step with web/src/i18n/languages.ts.
        constexpr const char *ENGLISH = "en";
        constexpr const char *CODES[] = {"en", "id", "de", "es", "fr", "it", "nl", "pl", "pt-BR", "tr", "ar", "ja", "ko", "zh-Hans"};

        constexpr size_t MAX_FILE_BYTES = 64 * 1024; // stored, gzip-compressed
        constexpr size_t FREE_SPACE_MARGIN = 4 * 1024;
        constexpr const char *REPO_RELEASES = "https://github.com/DeanJ87/SQMeter/releases/download/";

        bool isSupported(const std::string &code);

        // Release asset names, e.g. sqmeter-i18n-es.json.gz.
        std::string assetName(const std::string &code);
        // https://github.com/.../releases/download/v<version>/<asset>
        std::string assetUrl(const std::string &firmwareVersion, const std::string &asset);

        // The checksum file published beside each language file
        // (<asset>.sha256): "<sha256 hex> <size>". The device reads this
        // rather than the JSON manifest, so it needs no JSON parser for it.
        struct Checksum
        {
            std::string sha256; // lowercase hex
            size_t size = 0;
        };
        std::string checksumName(const std::string &asset);
        bool parseChecksum(const std::string &text, Checksum &out, std::string &error);

        // A file can be installed when it is gzip (magic 1f 8b), within the size
        // limit, and fits with a margin in the free space.
        enum class FileCheck
        {
            Ok,
            NotGzip,
            TooBig,
            NoSpace,
        };
        FileCheck checkFile(const uint8_t *head, size_t headLength, size_t size, size_t freeBytes);
        const char *fileCheckMessage(FileCheck check);
    } // namespace Language
} // namespace SQM
