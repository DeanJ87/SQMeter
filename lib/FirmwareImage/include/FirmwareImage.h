#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace SQM
{
    // Which partition layout and build a firmware image is for, and whether
    // this device may install it (specs/027-firmware-platform-and-space,
    // FR-020). Every image carries a marker: a fixed 16-byte magic followed by
    // "layout=<id>;build=<standard|ble>;" and a NUL. OTA scans the image as it
    // is written and refuses it before the boot partition is switched.
    namespace FirmwareImage
    {
        // The whole-chip layout of partitions.csv (spec 027).
        constexpr const char *LAYOUT = "l2";
        // Its app slot and LittleFS sizes, to recognise it at run time.
        constexpr uint32_t APP_SLOT_SIZE = 0x1C0000;
        constexpr uint32_t FS_SIZE = 0x70000;

        enum class Build
        {
            Standard,
            Ble,
        };

        const char *buildName(Build build);

        // "l2" when the running partition table is this layout, otherwise
        // "legacy" (a device moved to 3.x firmware without the USB flash).
        std::string layoutOf(uint32_t appSlotSize, uint32_t fsSize);

        constexpr size_t MAGIC_SIZE = 16;
        constexpr size_t MAX_FIELDS = 48;

        // The magic, stored scrambled so the scanner's own copy of it never
        // matches when an image is scanned.
        std::array<uint8_t, MAGIC_SIZE> magic();

        // The full marker for an image of this build (magic + fields + NUL),
        // as embedded by the firmware (src/FirmwareMarker.cpp).
        std::string marker(Build build);

        // Finds the marker in an image fed in chunks of any size.
        class Scanner
        {
          public:
            Scanner();
            void feed(const uint8_t *data, size_t len);
            bool found() const { return complete; }
            const std::string &layout() const { return layoutField; }
            // Empty when the marker had no (or an unknown) build.
            const std::string &build() const { return buildField; }

          private:
            void parseFields();

            std::array<uint8_t, MAGIC_SIZE> pattern{};
            size_t matched = 0;
            bool readingFields = false;
            bool complete = false;
            std::string fields;
            std::string layoutField;
            std::string buildField;
        };

        enum class Verdict
        {
            Ok,
            NoMarker,         // a 2.x image, or not SQMeter firmware
            NeedsUsbFlash,    // this device is still on the old layout
            OtherLayout,      // built for a layout this device doesn't have
            OtherBuild,       // standard image on a BLE device, or the reverse
        };

        Verdict check(const Scanner &scanner, const std::string &deviceLayout, Build deviceBuild);

        // One line for the person updating (DS-22), English; the UI
        // translates it through the device-message templates.
        const char *verdictMessage(Verdict verdict, Build deviceBuild);

        // A LittleFS image must fill this device's partition exactly.
        // `uploadBytes` is a multipart body's length: the image plus up to
        // `overhead` bytes of form framing.
        bool fsImageFits(size_t uploadBytes, size_t partitionSize, size_t overhead);
    } // namespace FirmwareImage
} // namespace SQM
