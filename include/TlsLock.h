#pragma once

#include <cstdint>

namespace SQM
{
    // One HTTPS session at a time. Each TLS session needs ~45 KB of heap,
    // including two large contiguous buffers; an alert send overlapping the
    // GitHub update check (separate tasks) could leave neither enough.
    namespace TlsLock
    {
        bool acquire(uint32_t timeoutMs);
        void release();

        class Guard
        {
        public:
            explicit Guard(uint32_t timeoutMs) : held(acquire(timeoutMs)) {}
            ~Guard()
            {
                if (held)
                    release();
            }
            Guard(const Guard &) = delete;
            Guard &operator=(const Guard &) = delete;
            bool ok() const { return held; }

        private:
            bool held;
        };
    } // namespace TlsLock
} // namespace SQM
