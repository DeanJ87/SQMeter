#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace SQM
{

    enum class SensorStatus
    {
        OK,
        NOT_INITIALIZED,
        READ_ERROR,
        TIMEOUT,
        INVALID_DATA
    };

    struct SensorReading
    {
        uint32_t timestamp;
        SensorStatus status;

        bool isValid() const { return status == SensorStatus::OK; }
    };

    class SensorBase
    {
    public:
        virtual ~SensorBase() = default;

        virtual bool begin() = 0;
        virtual void update() = 0;
        virtual std::string getName() const = 0;
        virtual std::string toJson() const = 0;

        bool isInitialized() const { return initialized; }
        uint32_t getLastUpdateTime() const { return lastUpdateTime; }

    protected:
        bool initialized = false;
        uint32_t lastUpdateTime = 0;

        static constexpr uint32_t SENSOR_TIMEOUT_MS = 5000;
    };

} // namespace SQM
