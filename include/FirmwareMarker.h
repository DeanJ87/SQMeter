#pragma once

#include "FirmwareImage.h"

#include <string>

namespace SQM
{
    // This firmware's own marker (lib/FirmwareImage, spec 027 FR-020) and the
    // partition layout it is running on.
    namespace FirmwareMarker
    {
        // The marker's text after the magic, e.g. "layout=l2;build=ble;".
        const char *fields();
        FirmwareImage::Build build();
        // "l2", or "legacy" when the partition table is an older layout.
        const std::string &layout();
    } // namespace FirmwareMarker
} // namespace SQM
