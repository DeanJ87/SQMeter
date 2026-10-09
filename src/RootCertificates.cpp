#include "AlertRootCA.h"

#include <cstddef>

namespace SQM
{
    namespace
    {
        // The roots only the full set (webhooks, self-hosted ntfy) uses.
#define SQM_CA_FULL_SET_ONLY                                                                                                               \
    SQM_CA_ISRG_ROOT_X2 SQM_CA_DIGICERT_GLOBAL_ROOT_CA SQM_CA_USERTRUST_RSA_CERTIFICATION_AUTHORITY SQM_CA_GTS_ROOT_R1 SQM_CA_GTS_ROOT_R4  \
        SQM_CA_AMAZON_ROOT_CA_1

        // Every root once; the smaller sets are its tail (see AlertRootCA.h).
        const char TRUSTED_ROOTS[] =
            SQM_CA_FULL_SET_ONLY SQM_CA_DIGICERT_GLOBAL_ROOT_G2 SQM_CA_USERTRUST_ECC_CERTIFICATION_AUTHORITY SQM_CA_ISRG_ROOT_X1;

        constexpr size_t PUSHOVER_OFFSET = sizeof(SQM_CA_FULL_SET_ONLY) - 1;
        constexpr size_t GITHUB_OFFSET = PUSHOVER_OFFSET + sizeof(SQM_CA_DIGICERT_GLOBAL_ROOT_G2) - 1;
        constexpr size_t NTFY_OFFSET = GITHUB_OFFSET + sizeof(SQM_CA_USERTRUST_ECC_CERTIFICATION_AUTHORITY) - 1;
#undef SQM_CA_FULL_SET_ONLY
    } // namespace

    const char *const ALERT_ROOT_CA_PEM = TRUSTED_ROOTS;
    const char *const PUSHOVER_ROOT_CA_PEM = TRUSTED_ROOTS + PUSHOVER_OFFSET;
    const char *const GITHUB_ROOT_CA_PEM = TRUSTED_ROOTS + GITHUB_OFFSET;
    const char *const NTFY_SH_ROOT_CA_PEM = TRUSTED_ROOTS + NTFY_OFFSET;
} // namespace SQM
