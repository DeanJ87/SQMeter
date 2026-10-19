#pragma once

// Hardware only the ESP32-S3 boards carry (specs 031, 032, 034): the OLED
// display and the Li-ion battery with its fuel gauge and charger. The S3 envs
// set both (platformio.ini [s3]); the classic esp32dev builds leave the
// drivers out to stay within the flash budget (SIZE-04). There the display is
// never probed and no gauge is found, so the API and web UI report them as not
// detected, and the settings stay valid for a later move to an S3 board.
#ifndef SQM_HAS_DISPLAY
#define SQM_HAS_DISPLAY 0
#endif
#ifndef SQM_HAS_BATTERY
#define SQM_HAS_BATTERY 0
#endif
