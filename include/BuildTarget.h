#pragma once

// The partition layout and release files this build is for (specs 027, 034),
// from the board flags the env sets in platformio.ini. The layout goes into the
// image marker (src/FirmwareMarker.cpp), so OTA only installs an image built
// for this device's partition table; the board picks this build's release
// files (lib/ReleaseLogic).
#if defined(SQM_BOARD_ESP32S3_ETH)
#define SQM_LAYOUT_ID "s16"
#define SQM_RELEASE_BOARD "esp32s3-eth"
#elif defined(SQM_BOARD_ESP32S3_ZERO) && defined(SQM_FLASH_8MB)
#define SQM_LAYOUT_ID "s8"
#define SQM_RELEASE_BOARD "esp32s3-zero8"
#elif defined(SQM_BOARD_ESP32S3_ZERO)
#define SQM_LAYOUT_ID "l2"
#define SQM_RELEASE_BOARD "esp32s3-zero"
#else
#define SQM_LAYOUT_ID "l2"
#define SQM_RELEASE_BOARD "esp32"
#endif
