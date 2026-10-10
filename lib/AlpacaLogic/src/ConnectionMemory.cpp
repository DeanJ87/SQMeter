#include "ConnectionMemory.h"

namespace SQM
{
    namespace Alpaca
    {
        namespace
        {
            constexpr uint32_t MAGIC = 0x53514D43; // "SQMC"

            uint8_t checkOf(uint8_t connected)
            {
                return static_cast<uint8_t>(~connected ^ 0x5A);
            }
        } // namespace

        bool keepsConnections(ResetKind kind)
        {
            return kind == ResetKind::Software || kind == ResetKind::Panic || kind == ResetKind::Watchdog;
        }

        ConnectionMemory rememberConnections(const bool (&connected)[DEVICE_COUNT])
        {
            ConnectionMemory memory;
            memory.magic = MAGIC;
            for (size_t i = 0; i < DEVICE_COUNT; ++i)
            {
                if (connected[i])
                    memory.connected = static_cast<uint8_t>(memory.connected | (1U << i));
            }
            memory.check = checkOf(memory.connected);
            return memory;
        }

        bool recallConnections(const ConnectionMemory &memory, bool (&connected)[DEVICE_COUNT])
        {
            const uint8_t validBits = static_cast<uint8_t>((1U << DEVICE_COUNT) - 1U);
            if (memory.magic != MAGIC || memory.check != checkOf(memory.connected) || (memory.connected & ~validBits) != 0)
                return false;
            for (size_t i = 0; i < DEVICE_COUNT; ++i)
                connected[i] = (memory.connected & (1U << i)) != 0;
            return true;
        }

    } // namespace Alpaca
} // namespace SQM
