#pragma once

#include <cstddef>
#include <cstdint>

namespace SQM
{
    // Free heap / largest free block at named points during boot, so memory
    // regressions show up in /api/status instead of as failed TLS handshakes.
    namespace HeapTrace
    {
        struct Checkpoint
        {
            const char *stage;
            uint32_t freeBytes;
            uint32_t largestBlock;
        };

        constexpr size_t MAX_CHECKPOINTS = 16;

        void mark(const char *stage);
        size_t count();
        const Checkpoint &at(size_t index);
    } // namespace HeapTrace
} // namespace SQM
