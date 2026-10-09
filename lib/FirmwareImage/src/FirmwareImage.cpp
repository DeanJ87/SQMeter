#include "FirmwareImage.h"

namespace SQM
{
    namespace FirmwareImage
    {
        namespace
        {
            // The magic XOR SCRAMBLE. Unscrambled only into a local copy, so the
            // magic bytes appear exactly once in an image: in its marker.
            constexpr uint8_t SCRAMBLE = 0xA5;
            constexpr std::array<uint8_t, MAGIC_SIZE> SCRAMBLED = {
                0xE6, 0xB4, 0x1D, 0x92, 0x5F, 0x0B, 0xC8, 0x37, 0x71, 0xAE, 0x24, 0xD9, 0x6C, 0x83, 0x3A, 0xF0};

            const char *LEGACY = "legacy";
        } // namespace

        const char *buildName(Build build)
        {
            return build == Build::Ble ? "ble" : "standard";
        }

        std::string layoutOf(uint32_t appSlotSize, uint32_t fsSize)
        {
            return appSlotSize == APP_SLOT_SIZE && fsSize == FS_SIZE ? LAYOUT : LEGACY;
        }

        std::array<uint8_t, MAGIC_SIZE> magic()
        {
            std::array<uint8_t, MAGIC_SIZE> out{};
            for (size_t i = 0; i < MAGIC_SIZE; ++i)
                out[i] = static_cast<uint8_t>(SCRAMBLED[i] ^ SCRAMBLE);
            return out;
        }

        std::string marker(Build build)
        {
            const auto bytes = magic();
            std::string out(bytes.begin(), bytes.end());
            out += std::string("layout=") + LAYOUT + ";build=" + buildName(build) + ";";
            out.push_back('\0');
            return out;
        }

        Scanner::Scanner() : pattern(magic()) {}

        void Scanner::feed(const uint8_t *data, size_t len)
        {
            for (size_t i = 0; i < len && !complete; ++i)
            {
                const uint8_t byte = data[i];
                if (readingFields)
                {
                    if (byte == 0 || fields.size() >= MAX_FIELDS)
                    {
                        readingFields = false;
                        parseFields();
                    }
                    else
                    {
                        fields.push_back(static_cast<char>(byte));
                    }
                    continue;
                }
                // The magic has no repeated prefix, so on a mismatch the only
                // possible restart is at this byte.
                if (byte == pattern[matched])
                    ++matched;
                else
                    matched = byte == pattern[0] ? 1 : 0;
                if (matched == MAGIC_SIZE)
                {
                    matched = 0;
                    readingFields = true;
                    fields.clear();
                }
            }
        }

        void Scanner::parseFields()
        {
            std::string layout;
            std::string build;
            size_t start = 0;
            while (start < fields.size())
            {
                size_t end = fields.find(';', start);
                if (end == std::string::npos)
                    end = fields.size();
                const std::string pair = fields.substr(start, end - start);
                const size_t eq = pair.find('=');
                if (eq != std::string::npos)
                {
                    const std::string key = pair.substr(0, eq);
                    const std::string value = pair.substr(eq + 1);
                    if (key == "layout")
                        layout = value;
                    else if (key == "build")
                        build = value;
                }
                start = end + 1;
            }
            // A stray copy of the magic without fields isn't a marker; keep looking.
            if (layout.empty())
                return;
            layoutField = layout;
            buildField = (build == "standard" || build == "ble") ? build : "";
            complete = true;
        }

        Verdict check(const Scanner &scanner, const std::string &deviceLayout, Build deviceBuild)
        {
            if (!scanner.found())
                return Verdict::NoMarker;
            if (scanner.layout() != deviceLayout)
                return deviceLayout == LEGACY && scanner.layout() == LAYOUT ? Verdict::NeedsUsbFlash : Verdict::OtherLayout;
            if (scanner.build() != buildName(deviceBuild))
                return Verdict::OtherBuild;
            return Verdict::Ok;
        }

        const char *verdictMessage(Verdict verdict, Build deviceBuild)
        {
            switch (verdict)
            {
            case Verdict::Ok:
                return "";
            case Verdict::NoMarker:
                return "Not firmware for this device. Use a v0.3 or later release file.";
            case Verdict::NeedsUsbFlash:
                return "This device needs the one-time USB flash first.";
            case Verdict::OtherLayout:
                return "Built for a different partition layout.";
            case Verdict::OtherBuild:
                return deviceBuild == Build::Ble ? "This is the standard build; this device runs the Bluetooth build."
                                                 : "This is the Bluetooth build; this device runs the standard build.";
            }
            return "";
        }

        bool fsImageFits(size_t uploadBytes, size_t partitionSize, size_t overhead)
        {
            return uploadBytes >= partitionSize && uploadBytes <= partitionSize + overhead;
        }
    } // namespace FirmwareImage
} // namespace SQM
