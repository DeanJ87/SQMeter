#include "FirmwareMarker.h"

#include <esp_partition.h>

// The marker every image carries (lib/FirmwareImage). The magic is spelled out
// here and nowhere else, so an image contains it exactly once; the native test
// test_firmware_image checks it matches FirmwareImage::marker().
#define SQM_MARKER_MAGIC "\x43\x11\xB8\x37\xFA\xAE\x6D\x92\xD4\x0B\x81\x7C\xC9\x26\x9F\x55"
#if SQM_ENABLE_BLE
#define SQM_MARKER_FIELDS "layout=l2;build=ble;"
#else
#define SQM_MARKER_FIELDS "layout=l2;build=standard;"
#endif

extern "C" __attribute__((used)) const char SQM_FIRMWARE_MARKER[] = SQM_MARKER_MAGIC SQM_MARKER_FIELDS;

namespace SQM
{
    namespace FirmwareMarker
    {
        const char *fields()
        {
            return SQM_FIRMWARE_MARKER + FirmwareImage::MAGIC_SIZE;
        }

        FirmwareImage::Build build()
        {
#if SQM_ENABLE_BLE
            return FirmwareImage::Build::Ble;
#else
            return FirmwareImage::Build::Standard;
#endif
        }

        const std::string &layout()
        {
            static const std::string value = []()
            {
                const esp_partition_t *app = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
                const esp_partition_t *fs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
                return FirmwareImage::layoutOf(app ? app->size : 0, fs ? fs->size : 0);
            }();
            return value;
        }
    } // namespace FirmwareMarker
} // namespace SQM
