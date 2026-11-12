#include "TcpDiagnostics.h"
#include "ConnectionBudget.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/priv/tcp_priv.h>
#include <lwip/tcpip.h>

namespace SQM
{
    namespace TcpDiagnostics
    {
        namespace
        {
            static_assert(static_cast<int>(WebHealth::TcpState::Listen) == LISTEN, "lwIP state order changed");
            static_assert(static_cast<int>(WebHealth::TcpState::FinWait2) == FIN_WAIT_2, "lwIP state order changed");
            static_assert(static_cast<int>(WebHealth::TcpState::TimeWait) == TIME_WAIT, "lwIP state order changed");
            static_assert(WebHealth::TCP_STATE_COUNT == TIME_WAIT + 1, "lwIP state count changed");
            static_assert(ConnectionBudget::TCP_CONNECTIONS == CONFIG_LWIP_MAX_ACTIVE_TCP, "ConnectionBudget out of step with sdkconfig");

            // As AsyncTCP does: take the core lock unless this task holds it.
            class CoreLock
            {
            public:
                CoreLock()
                    : held(!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER))
                {
                    if (held)
                        LOCK_TCPIP_CORE();
                }
                ~CoreLock()
                {
                    if (held)
                        UNLOCK_TCPIP_CORE();
                }
                CoreLock(const CoreLock &) = delete;
                CoreLock &operator=(const CoreLock &) = delete;

            private:
                bool held;
            };

            void addList(WebHealth::TcpCounts &counts, const tcp_pcb *pcb)
            {
                for (; pcb != nullptr; pcb = pcb->next)
                    counts.add(static_cast<uint8_t>(pcb->state));
            }
        } // namespace

        WebHealth::TcpCounts count()
        {
            WebHealth::TcpCounts counts;
            CoreLock lock;
            addList(counts, tcp_active_pcbs);
            addList(counts, tcp_tw_pcbs);
            addList(counts, tcp_bound_pcbs);
            for (const tcp_pcb_listen *pcb = tcp_listen_pcbs.listen_pcbs; pcb != nullptr; pcb = pcb->next)
            {
                counts.add(static_cast<uint8_t>(pcb->state));
                const unsigned backlog = counts.listenBacklog + pcb->accepts_pending;
                counts.listenBacklog = static_cast<uint8_t>(backlog > UINT8_MAX ? UINT8_MAX : backlog);
            }
            return counts;
        }

        WebHealth::TaskActivity asyncTcpActivity(uint32_t &stackFree)
        {
            // AsyncTCP creates the task when the first server or client starts.
            static TaskHandle_t task = nullptr;
            if (task == nullptr)
                task = xTaskGetHandle("async_tcp");
            if (task == nullptr)
            {
                stackFree = 0;
                return WebHealth::TaskActivity::Unknown;
            }
            stackFree = uxTaskGetStackHighWaterMark(task);
            switch (eTaskGetState(task))
            {
            case eRunning:
            case eReady:
                return WebHealth::TaskActivity::Busy;
            case eBlocked:
            case eSuspended:
                return WebHealth::TaskActivity::Idle;
            default:
                return WebHealth::TaskActivity::Unknown;
            }
        }
    } // namespace TcpDiagnostics
} // namespace SQM
