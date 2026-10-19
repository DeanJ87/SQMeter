#pragma once

#include <cstdint>

// The board a firmware build is for, and that board's default pins and usable
// GPIOs. The build picks one with -DSQM_BOARD_ESP32S3_ETH or
// -DSQM_BOARD_ESP32S3_ZERO (platformio.ini); without either it is the classic
// ESP32 DevKit. Config::createDefault() and Config::validate() use it, so a
// fresh device starts with its board's wiring (SQMeter-Hardware
// hardware/kicad/README.md, "GPIO maps").

namespace SQM
{
    enum class Board : uint8_t
    {
        Esp32Dev,    // classic ESP32 DevKit (esp32dev, esp32dev-ble)
        Esp32S3Eth,  // Waveshare ESP32-S3-ETH: main rig, W5500 on GPIO 9-14
        Esp32S3Zero, // Waveshare ESP32-S3-Zero (4 MB) and Zero-N8R8 (8 MB): travel rig
    };

    struct BoardProfile
    {
        static constexpr uint8_t NONE = 255; // not wired on this board

        uint8_t i2cSDA = NONE;
        uint8_t i2cSCL = NONE;
        uint8_t i2c2SDA = NONE; // NONE = no second bus wired
        uint8_t i2c2SCL = NONE;
        uint8_t gpsRx = NONE; // ESP side
        uint8_t gpsTx = NONE;
        uint8_t rainRx = NONE; // ESP side
        uint8_t rainTx = NONE;
        uint8_t windSpeed = NONE;
        uint8_t windDirection = NONE;
        uint8_t aux1 = NONE;
        uint8_t aux2 = NONE;
        // Travel board power monitoring (MAX17048 ALRT, charger STAT1/STAT2/PG;
        // all active low). Reference wiring for the battery settings (spec 032).
        uint8_t batteryAlert = NONE;
        uint8_t chargerStat1 = NONE;
        uint8_t chargerStat2 = NONE;
        uint8_t chargerPowerGood = NONE;
        uint8_t statusLed = NONE; // on-board RGB LED (not driven yet)
        uint64_t validGpios = 0;  // bit n set = GPIO n can carry a sensor bus
        uint64_t rtcGpios = 0;    // bit n set = GPIO n can wake the chip from deep sleep (ext0)
        // The wind vane needs ADC1 (ADC2 can't be read while WiFi is on).
        uint8_t adc1First = NONE;
        uint8_t adc1Last = NONE;
        const char *adc1Pins = ""; // the same range as text, for the settings error

        constexpr bool isValidGpio(int pin) const { return pin >= 0 && pin < 64 && ((validGpios >> pin) & 1ULL) != 0; }
        constexpr bool isRtcGpio(int pin) const { return pin >= 0 && pin < 64 && ((rtcGpios >> pin) & 1ULL) != 0; }
        constexpr bool isAdc1(int pin) const { return pin >= adc1First && pin <= adc1Last; }
    };

    namespace BoardDetail
    {
        constexpr uint64_t gpioBit(int pin)
        {
            return 1ULL << pin;
        }
        constexpr uint64_t gpioRange(int first, int last)
        {
            return first > last ? 0 : gpioBit(first) | gpioRange(first + 1, last);
        }

        // Classic ESP32: no flash pins (6-11), no 20/24/28-31.
        constexpr uint64_t ESP32_GPIOS =
            gpioRange(0, 5) | gpioRange(12, 19) | gpioRange(21, 23) | gpioRange(25, 27) | gpioRange(32, 36) | gpioBit(39);
        // ESP32-S3: no USB (19/20), the RGB LED (21), flash and octal PSRAM (26-37).
        constexpr uint64_t S3_GPIOS = gpioRange(0, 18) | gpioRange(38, 48);
        // RTC GPIOs (deep-sleep wake sources).
        constexpr uint64_t ESP32_RTC_GPIOS =
            gpioBit(0) | gpioBit(2) | gpioBit(4) | gpioRange(12, 15) | gpioRange(25, 27) | gpioRange(32, 39);
        constexpr uint64_t S3_RTC_GPIOS = gpioRange(0, 21);

        constexpr BoardProfile esp32Dev()
        {
            BoardProfile p;
            p.i2cSDA = 21;
            p.i2cSCL = 22;
            p.gpsRx = 17;
            p.gpsTx = 16;
            p.rainRx = 18;
            p.rainTx = 19;
            p.windSpeed = 27;
            p.windDirection = 35;
            p.validGpios = ESP32_GPIOS;
            p.rtcGpios = ESP32_RTC_GPIOS;
            p.adc1First = 32;
            p.adc1Last = 39;
            p.adc1Pins = "32-39";
            return p;
        }

        // The W5500 owns GPIO 9-14 (not usable for sensors); the SD slot is on 4-7.
        constexpr BoardProfile esp32S3Eth()
        {
            BoardProfile p;
            p.i2cSDA = 17;
            p.i2cSCL = 18;
            p.i2c2SDA = 41;
            p.i2c2SCL = 42;
            p.gpsRx = 48;
            p.gpsTx = 47;
            p.rainRx = 16;
            p.rainTx = 15;
            p.windSpeed = 2;
            p.windDirection = 1;
            p.aux1 = 39;
            p.aux2 = 40;
            p.statusLed = 21;
            p.validGpios = S3_GPIOS & ~gpioRange(9, 14);
            p.rtcGpios = S3_RTC_GPIOS;
            p.adc1First = 1;
            p.adc1Last = 10;
            p.adc1Pins = "1-10";
            return p;
        }

        // The travel board has no RG-15 or wind inputs: their defaults are the
        // shared Zero/DevKit map (RG-15 1/2, wind 4/5) and stay off, because
        // on the travel board those pins carry the fuel gauge and charger.
        constexpr BoardProfile esp32S3Zero()
        {
            BoardProfile p;
            p.i2cSDA = 8;
            p.i2cSCL = 9;
            p.i2c2SDA = 10;
            p.i2c2SCL = 11;
            p.gpsRx = 13;
            p.gpsTx = 12;
            p.rainRx = 2;
            p.rainTx = 1;
            p.windSpeed = 5;
            p.windDirection = 4;
            p.aux1 = 6;
            p.aux2 = 7;
            p.batteryAlert = 1;
            p.chargerStat1 = 2;
            p.chargerStat2 = 4;
            p.chargerPowerGood = 5;
            p.statusLed = 21;
            p.validGpios = S3_GPIOS;
            p.rtcGpios = S3_RTC_GPIOS;
            p.adc1First = 1;
            p.adc1Last = 10;
            p.adc1Pins = "1-10";
            return p;
        }
    } // namespace BoardDetail

    inline constexpr BoardProfile ESP32_DEV_PROFILE = BoardDetail::esp32Dev();
    inline constexpr BoardProfile ESP32S3_ETH_PROFILE = BoardDetail::esp32S3Eth();
    inline constexpr BoardProfile ESP32S3_ZERO_PROFILE = BoardDetail::esp32S3Zero();

    constexpr const BoardProfile &boardProfile(Board board)
    {
        return board == Board::Esp32S3Eth ? ESP32S3_ETH_PROFILE : board == Board::Esp32S3Zero ? ESP32S3_ZERO_PROFILE : ESP32_DEV_PROFILE;
    }

#if defined(SQM_BOARD_ESP32S3_ETH)
    inline constexpr Board BUILD_BOARD = Board::Esp32S3Eth;
#elif defined(SQM_BOARD_ESP32S3_ZERO)
    inline constexpr Board BUILD_BOARD = Board::Esp32S3Zero;
#else
    inline constexpr Board BUILD_BOARD = Board::Esp32Dev;
#endif
} // namespace SQM
