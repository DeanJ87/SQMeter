#include "HeapTrace.h"
#include "Logger.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace SQM
{
    namespace HeapTrace
    {
        namespace
        {
            Checkpoint checkpoints[MAX_CHECKPOINTS];
            size_t used = 0;
        } // namespace

        void mark(const char *stage)
        {
            const uint32_t freeBytes = ESP.getFreeHeap();
            const uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            Logger::info(
                "Heap",
                "%-24s free %6u  largest block %6u  stack left %5u",
                stage,
                freeBytes,
                largest,
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            if (used < MAX_CHECKPOINTS)
                checkpoints[used++] = {stage, freeBytes, largest};
        }

        size_t count()
        {
            return used;
        }
        const Checkpoint &at(size_t index)
        {
            return checkpoints[index];
        }
    } // namespace HeapTrace
} // namespace SQM
