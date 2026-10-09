#include "LanguageLogic.h"

#include <cctype>
#include <cstdlib>

namespace SQM
{
    namespace Language
    {
        bool isSupported(const std::string &code)
        {
            for (const char *known : CODES)
                if (code == known)
                    return true;
            return false;
        }

        std::string assetName(const std::string &code)
        {
            return "sqmeter-i18n-" + code + ".json.gz";
        }

        std::string assetUrl(const std::string &firmwareVersion, const std::string &asset)
        {
            return std::string(REPO_RELEASES) + "v" + firmwareVersion + "/" + asset;
        }

        namespace
        {
            bool isHex64(const char *value)
            {
                if (!value)
                    return false;
                size_t n = 0;
                for (; value[n]; ++n)
                    if (!std::isxdigit(static_cast<unsigned char>(value[n])))
                        return false;
                return n == 64;
            }
        } // namespace

        std::string checksumName(const std::string &asset)
        {
            return asset + ".sha256";
        }

        bool parseChecksum(const std::string &text, Checksum &out, std::string &error)
        {
            const size_t space = text.find(' ');
            const std::string hash = text.substr(0, space);
            unsigned long size = 0;
            if (space != std::string::npos)
                size = std::strtoul(text.c_str() + space + 1, nullptr, 10);
            if (!isHex64(hash.c_str()) || size == 0 || size > MAX_FILE_BYTES)
            {
                error = "The language file's checksum is missing or damaged";
                return false;
            }
            out.sha256 = hash;
            for (char &c : out.sha256)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            out.size = size;
            return true;
        }

        FileCheck checkFile(const uint8_t *head, size_t headLength, size_t size, size_t freeBytes)
        {
            if (headLength < 2 || head[0] != 0x1f || head[1] != 0x8b)
                return FileCheck::NotGzip;
            if (size > MAX_FILE_BYTES)
                return FileCheck::TooBig;
            if (size + FREE_SPACE_MARGIN > freeBytes)
                return FileCheck::NoSpace;
            return FileCheck::Ok;
        }

        const char *fileCheckMessage(FileCheck check)
        {
            switch (check)
            {
            case FileCheck::NotGzip:
                return "That isn't a SQMeter language file (.json.gz)";
            case FileCheck::TooBig:
                return "The language file is too big (64 KB at most)";
            case FileCheck::NoSpace:
                return "Not enough free space on the device for the language file";
            case FileCheck::Ok:
            default:
                return "";
            }
        }

        BootAction bootAction(const std::string &code, bool installedMatches, bool unfinishedAttempt)
        {
            if (code == ENGLISH)
                return BootAction::Nothing;
            if (installedMatches)
                return BootAction::UseInstalled;
            return unfinishedAttempt ? BootAction::WaitAfterCrash : BootAction::Restore;
        }
    } // namespace Language
} // namespace SQM
