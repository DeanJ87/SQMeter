#include "TlsLock.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SQM
{
    namespace TlsLock
    {
        namespace
        {
            SemaphoreHandle_t lock()
            {
                static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
                return mutex;
            }
        }

        bool acquire(uint32_t timeoutMs)
        {
            return xSemaphoreTake(lock(), pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
        }

        void release()
        {
            xSemaphoreGive(lock());
        }
    } // namespace TlsLock
} // namespace SQM
