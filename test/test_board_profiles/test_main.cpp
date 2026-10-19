#include <unity.h>

#include <string>

#include "BoardProfile.h"
#include "Config.h"

// Board pin defaults: a fresh device starts with its board's wiring
// (SQMeter-Hardware hardware/kicad/README.md, "GPIO maps").

using namespace SQM;

namespace
{
    void assertValidFor(Board board)
    {
        const Config cfg = Config::createDefault(board);
        std::string error;
        TEST_ASSERT_TRUE_MESSAGE(cfg.validate(boardProfile(board), &error), error.c_str());
    }

    // Every feature switched on at its default pins: no two share a pin.
    void assertEverythingOnIsValid(Board board)
    {
        Config cfg = Config::createDefault(board);
        cfg.gps.enabled = true;
        cfg.rain.enabled = true;
        cfg.wind.enabled = true;
        cfg.wind.directionEnabled = true;
        cfg.sky.sqmlEnabled = true;
        std::string error;
        TEST_ASSERT_TRUE_MESSAGE(cfg.validate(boardProfile(board), &error), error.c_str());
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_classic_defaults_unchanged()
{
    const Config cfg = Config::createDefault(Board::Esp32Dev);
    TEST_ASSERT_EQUAL_UINT8(21, cfg.sensor.i2cSDA);
    TEST_ASSERT_EQUAL_UINT8(22, cfg.sensor.i2cSCL);
    TEST_ASSERT_FALSE(cfg.sensor.i2c2Configured());
    TEST_ASSERT_EQUAL_UINT8(17, cfg.gps.rxPin);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.gps.txPin);
    TEST_ASSERT_EQUAL_UINT8(18, cfg.rain.rxPin);
    TEST_ASSERT_EQUAL_UINT8(19, cfg.rain.txPin);
    TEST_ASSERT_EQUAL_UINT8(27, cfg.wind.speedPin);
    TEST_ASSERT_EQUAL_UINT8(35, cfg.wind.directionPin);
    assertValidFor(Board::Esp32Dev);
}

void test_native_build_is_classic()
{
    TEST_ASSERT_TRUE(BUILD_BOARD == Board::Esp32Dev);
    const Config built = Config::createDefault();
    TEST_ASSERT_EQUAL_UINT8(21, built.sensor.i2cSDA);
}

void test_eth_defaults()
{
    const Config cfg = Config::createDefault(Board::Esp32S3Eth);
    TEST_ASSERT_EQUAL_UINT8(17, cfg.sensor.i2cSDA);
    TEST_ASSERT_EQUAL_UINT8(18, cfg.sensor.i2cSCL);
    TEST_ASSERT_EQUAL_UINT8(41, cfg.sensor.i2c2SDA);
    TEST_ASSERT_EQUAL_UINT8(42, cfg.sensor.i2c2SCL);
    TEST_ASSERT_EQUAL_UINT8(48, cfg.gps.rxPin);
    TEST_ASSERT_EQUAL_UINT8(47, cfg.gps.txPin);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.rain.rxPin);
    TEST_ASSERT_EQUAL_UINT8(15, cfg.rain.txPin);
    TEST_ASSERT_EQUAL_UINT8(2, cfg.wind.speedPin);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.wind.directionPin);
    const BoardProfile &eth = boardProfile(Board::Esp32S3Eth);
    TEST_ASSERT_EQUAL_UINT8(39, eth.aux1);
    TEST_ASSERT_EQUAL_UINT8(40, eth.aux2);
    TEST_ASSERT_EQUAL_UINT8(BoardProfile::NONE, eth.batteryAlert);
    assertValidFor(Board::Esp32S3Eth);
    assertEverythingOnIsValid(Board::Esp32S3Eth);
}

void test_zero_defaults()
{
    const Config cfg = Config::createDefault(Board::Esp32S3Zero);
    TEST_ASSERT_EQUAL_UINT8(8, cfg.sensor.i2cSDA);
    TEST_ASSERT_EQUAL_UINT8(9, cfg.sensor.i2cSCL);
    TEST_ASSERT_EQUAL_UINT8(10, cfg.sensor.i2c2SDA);
    TEST_ASSERT_EQUAL_UINT8(11, cfg.sensor.i2c2SCL);
    TEST_ASSERT_EQUAL_UINT8(13, cfg.gps.rxPin);
    TEST_ASSERT_EQUAL_UINT8(12, cfg.gps.txPin);
    TEST_ASSERT_FALSE(cfg.rain.enabled);
    TEST_ASSERT_FALSE(cfg.wind.enabled);
    const BoardProfile &zero = boardProfile(Board::Esp32S3Zero);
    TEST_ASSERT_EQUAL_UINT8(6, zero.aux1);
    TEST_ASSERT_EQUAL_UINT8(7, zero.aux2);
    TEST_ASSERT_EQUAL_UINT8(1, zero.batteryAlert);
    TEST_ASSERT_EQUAL_UINT8(2, zero.chargerStat1);
    TEST_ASSERT_EQUAL_UINT8(4, zero.chargerStat2);
    TEST_ASSERT_EQUAL_UINT8(5, zero.chargerPowerGood);
    assertValidFor(Board::Esp32S3Zero);
    assertEverythingOnIsValid(Board::Esp32S3Zero);
}

void test_gpio_rules_follow_the_board()
{
    const BoardProfile &classic = boardProfile(Board::Esp32Dev);
    const BoardProfile &eth = boardProfile(Board::Esp32S3Eth);
    const BoardProfile &zero = boardProfile(Board::Esp32S3Zero);
    // Classic flash pins; S3 USB, RGB LED, flash/octal PSRAM.
    TEST_ASSERT_FALSE(classic.isValidGpio(8));
    TEST_ASSERT_TRUE(zero.isValidGpio(8));
    for (int pin : {19, 20, 21, 26, 32, 33, 37})
        TEST_ASSERT_FALSE(zero.isValidGpio(pin));
    TEST_ASSERT_TRUE(eth.isValidGpio(48));
    TEST_ASSERT_FALSE(classic.isValidGpio(48));
    // The W5500 owns 9-14 on the ETH board.
    for (int pin = 9; pin <= 14; ++pin)
        TEST_ASSERT_FALSE(eth.isValidGpio(pin));

    // An S3 pin is rejected by the classic rules and accepted by the S3 ones.
    Config cfg = Config::createDefault(Board::Esp32S3Zero);
    std::string error;
    TEST_ASSERT_FALSE(cfg.validate(classic, &error));
    TEST_ASSERT_EQUAL_STRING("I2C pins are invalid", error.c_str());

    // The vane needs ADC1: GPIO 1-10 on the S3.
    cfg = Config::createDefault(Board::Esp32S3Eth);
    cfg.wind.enabled = true;
    cfg.wind.directionEnabled = true;
    cfg.wind.directionPin = 39;
    TEST_ASSERT_FALSE(cfg.validate(eth, &error));
    TEST_ASSERT_EQUAL_STRING("Wind: vane pin must be an ADC1 pin (GPIO 1-10)", error.c_str());

    Config classicCfg = Config::createDefault(Board::Esp32Dev);
    classicCfg.wind.enabled = true;
    classicCfg.wind.directionEnabled = true;
    classicCfg.wind.directionPin = 4;
    TEST_ASSERT_FALSE(classicCfg.validate(classic, &error));
    TEST_ASSERT_EQUAL_STRING("Wind: vane pin must be an ADC1 pin (GPIO 32-39)", error.c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_classic_defaults_unchanged);
    RUN_TEST(test_native_build_is_classic);
    RUN_TEST(test_eth_defaults);
    RUN_TEST(test_zero_defaults);
    RUN_TEST(test_gpio_rules_follow_the_board);
    return UNITY_END();
}
