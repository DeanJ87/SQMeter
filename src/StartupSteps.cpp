#include "StartupSteps.h"

#include "HeapTrace.h"
#include "Logger.h"

#include <ESPmDNS.h>
#include <esp_ota_ops.h>

namespace SQM
{
    namespace StartupSteps
    {
        namespace
        {
            constexpr const char *TAG = "Main";
            constexpr uint16_t ARDUINO_OTA_PORT = 3232;

            unsigned long sinceBootMs()
            {
                return static_cast<unsigned long>(millis());
            }

            // Spec 027 FR-021: a new firmware is kept once its core is up
            // (config, WiFi, the web server with the update endpoints, a loop
            // pass), before the sensors and radios start, so a supply that
            // sags while they start can't roll a working firmware back. One
            // that fails before this point rolls back on its next boot.
            void confirmImage(const StartupDevices &)
            {
                esp_ota_mark_app_valid_cancel_rollback();
                Logger::info(TAG, "Firmware confirmed %lu ms after boot", sinceBootMs());
            }

            void startRain(const StartupDevices &d)
            {
                // A settings save while waiting may have started (or stopped) it already.
                if (d.config.rain.enabled && !d.rain.isInitialized() && !d.rain.begin())
                    Logger::warn(TAG, "RG-15 initialization failed");
            }

            void startSky(const StartupDevices &d)
            {
                for (TSL2591Sensor *sky : {&d.sqm, &d.sqml})
                {
                    if (d.skySensorActive(sky->role()) && !sky->begin())
                        Logger::error(TAG, "Failed to initialize %s", skyRoleLabel(sky->role()));
                }
            }

            void startInfrared(const StartupDevices &d)
            {
                if (!d.mlx.begin())
                    Logger::warn(TAG, "MLX90614 not detected (optional sensor)");
            }

            void startEnvironment(const StartupDevices &d)
            {
                if (!d.bme.begin())
                    Logger::warn(TAG, "BME280 not detected (optional sensor)");
            }

            void startGps(const StartupDevices &d)
            {
                if (!d.gps.begin())
                    Logger::warn(TAG, "GPS initialization failed");
            }

            void startWind(const StartupDevices &d)
            {
                d.wind.begin();
            }
            void startBattery(const StartupDevices &d)
            {
                d.battery.begin(d.config.battery);
            }

            // Nothing to begin: the OLED's first bus traffic, and the regular
            // sensor reads, start with the next loop pass.
            void startDisplay(const StartupDevices &)
            {
                Logger::info(TAG, "Sensors started %lu ms after boot", sinceBootMs());
            }

            void startTime(const StartupDevices &d)
            {
                if (d.wifi.isConnected())
                    d.time.begin();
            }

            void startMqtt(const StartupDevices &d)
            {
                d.mqtt.begin();
                HeapTrace::mark("mqtt");
            }

            void startMdns(const StartupDevices &d)
            {
                d.wifi.allowMdns();
                if (d.arduinoOtaEnabled && d.wifi.isMdnsRunning())
                    MDNS.enableArduino(ARDUINO_OTA_PORT, true);
            }

            void startDiscovery(const StartupDevices &d)
            {
                d.web.startAlpacaDiscovery();
            }
            void startBluetooth(const StartupDevices &d)
            {
                d.web.startBluetooth();
            }

            void restoreWifiPower(const StartupDevices &d)
            {
                d.wifi.restoreTxPower();
                HeapTrace::mark("started");
                Logger::info(TAG, "Start-up complete %lu ms after boot", sinceBootMs());
            }

            using Function = void (*)(const StartupDevices &);
            // In Boot::Step order.
            constexpr Function IN_ORDER[] = {
                confirmImage,
                startRain,
                startSky,
                startInfrared,
                startEnvironment,
                startGps,
                startWind,
                startBattery,
                startDisplay,
                startTime,
                startMqtt,
                startMdns,
                startDiscovery,
                startBluetooth,
                restoreWifiPower,
            };
            static_assert(sizeof(IN_ORDER) / sizeof(IN_ORDER[0]) == Boot::STEP_COUNT, "one function per start-up step");
        } // namespace

        uint32_t enabledSteps(const StartupDevices &d)
        {
            uint32_t steps = Boot::ALL_STEPS;
            if (!d.config.rain.enabled)
                steps &= ~Boot::stepBit(Boot::Step::Rain);
            if (!d.config.gps.enabled)
                steps &= ~Boot::stepBit(Boot::Step::Gps);
            if (!d.config.alpaca.enabled)
                steps &= ~Boot::stepBit(Boot::Step::AlpacaDiscovery);
            if (!d.web.bluetoothWanted())
                steps &= ~Boot::stepBit(Boot::Step::Bluetooth);
            return steps;
        }

        void run(Boot::Step step, const StartupDevices &devices)
        {
            const auto index = static_cast<uint8_t>(step);
            if (index < Boot::STEP_COUNT)
                IN_ORDER[index](devices);
        }
    } // namespace StartupSteps
} // namespace SQM
